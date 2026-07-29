#include <nrf_log.h>
#include <algorithm>
#include <cstring>
#include <host/ble_att.h>
#include "FSService.h"
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/settings/Settings.h"
#include "systemtask/SystemTask.h"

using namespace Pinetime::Controllers;

constexpr ble_uuid16_t FSService::fsServiceUuid;
constexpr ble_uuid128_t FSService::fsVersionUuid;
constexpr ble_uuid128_t FSService::fsTransferUuid;

int FSServiceCallback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
  auto* fsService = static_cast<FSService*>(arg);
  return fsService->OnFSServiceRequested(conn_handle, attr_handle, ctxt);
}

FSService::FSService(Pinetime::System::SystemTask& systemTask, Pinetime::Controllers::FS& fs)
  : systemTask {systemTask},
    fs {fs},
    characteristicDefinition {{.uuid = &fsVersionUuid.u,
                               .access_cb = FSServiceCallback,
                               .arg = this,
                               .flags = BLE_GATT_CHR_F_READ,
                               .val_handle = &versionCharacteristicHandle},
                              {
                                .uuid = &fsTransferUuid.u,
                                .access_cb = FSServiceCallback,
                                .arg = this,
                                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                                .val_handle = &transferCharacteristicHandle,
                              },
                              {0}},
    serviceDefinition {
      {/* Device Information Service */
       .type = BLE_GATT_SVC_TYPE_PRIMARY,
       .uuid = &fsServiceUuid.u,
       .characteristics = characteristicDefinition},
      {0},
    } {
}

void FSService::Init() {
  int res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);
}

uint32_t FSService::MaxReadChunkBytes(uint16_t connectionHandle) const {
  uint16_t mtu = ble_att_mtu(connectionHandle);
  if (mtu < 23) {
    mtu = 23;
  }
  // ATT header + fixed ReadResponse fields (flexible chunk[] has size 0 in sizeof).
  if (mtu <= attHeaderBytes + sizeof(ReadResponse)) {
    return 0;
  }
  uint32_t room = static_cast<uint32_t>(mtu) - attHeaderBytes - sizeof(ReadResponse);
  return std::min(room, static_cast<uint32_t>(maxChunkPayload));
}

void FSService::OnDisconnect() {
  FinishListDir();
}

void FSService::OnNotifyTxComplete(uint16_t attributeHandle) {
  if (!listDir.active || attributeHandle != transferCharacteristicHandle) {
    return;
  }
  if (listDir.finishing) {
    FinishListDir();
    return;
  }
  SendListDirEntry(false);
}

void FSService::FinishListDir() {
  if (!listDir.active) {
    return;
  }
  fs.DirClose(&listDir.dir);
  listDir = {};
  systemTask.PushMessage(Pinetime::System::Messages::StopFileTransfer);
}

void FSService::SendListDirEntry(bool endOfList) {
  ListDirResponse resp {};
  resp.command = commands::LISTDIR_ENTRY;
  resp.status = 0x01;
  resp.totalentries = listDir.totalentries;
  resp.entry = listDir.entry;
  resp.modification_time = 0;

  if (endOfList) {
    resp.file_size = 0;
    resp.path_length = 0;
    resp.flags = 0;
    auto* om = ble_hs_mbuf_from_flat(&resp, sizeof(ListDirResponse));
    ble_gattc_notify_custom(listDir.connectionHandle, transferCharacteristicHandle, om);
    listDir.finishing = true;
    return;
  }

  lfs_info info = {};
  int res = fs.DirRead(&listDir.dir, &info);
  if (res <= 0) {
    SendListDirEntry(true);
    return;
  }

  switch (info.type) {
    case LFS_TYPE_REG:
      resp.flags = 0;
      resp.file_size = info.size;
      break;
    case LFS_TYPE_DIR:
      resp.flags = 1;
      resp.file_size = 0;
      break;
    default:
      resp.flags = 0;
      resp.file_size = 0;
      break;
  }

  resp.path_length = strlen(info.name);
  auto* om = ble_hs_mbuf_from_flat(&resp, sizeof(ListDirResponse));
  os_mbuf_append(om, info.name, resp.path_length);
  ble_gattc_notify_custom(listDir.connectionHandle, transferCharacteristicHandle, om);
  listDir.entry++;
}

int FSService::OnFSServiceRequested(uint16_t connectionHandle, uint16_t attributeHandle, ble_gatt_access_ctxt* context) {
#ifndef PINETIME_IS_RECOVERY
  if (systemTask.GetSettings().GetDfuAndFsMode() == Pinetime::Controllers::Settings::DfuAndFsMode::Disabled) {
    Pinetime::Controllers::NotificationManager::Notification notif;
    memcpy(notif.message.data(), denyAlert, denyAlertLength);
    notif.size = denyAlertLength;
    notif.category = Pinetime::Controllers::NotificationManager::Categories::SimpleAlert;
    if (systemTask.GetNotificationManager().PushIfNew(std::move(notif))) {
      systemTask.PushMessage(Pinetime::System::Messages::OnNewNotification);
    }
    return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
  }
#endif

  if (attributeHandle == versionCharacteristicHandle) {
    NRF_LOG_INFO("FS_S : handle = %d", versionCharacteristicHandle);
    int res = os_mbuf_append(context->om, &fsVersion, sizeof(fsVersion));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (attributeHandle == transferCharacteristicHandle) {
    return FSCommandHandler(connectionHandle, context->om);
  }
  return 0;
}

int FSService::FSCommandHandler(uint16_t connectionHandle, os_mbuf* om) {
  auto command = static_cast<commands>(om->om_data[0]);
  NRF_LOG_INFO("[FS_S] -> FSCommandHandler Command %d", command);
  // Just always make sure we are awake...
  systemTask.PushMessage(Pinetime::System::Messages::StartFileTransfer);
  vTaskDelay(10);
  while (systemTask.IsSleeping()) {
    vTaskDelay(100);
  }
  lfs_info info = {0};
  lfs_file f = {0};
  uint8_t fileData[maxChunkPayload] = {0};
  switch (command) {
    case commands::READ: {
      NRF_LOG_INFO("[FS_S] -> Read");
      auto* header = (ReadHeader*) om->om_data;
      uint16_t plen = header->pathlen;
      if (plen > maxpathlen) {
        ReadResponse resp {};
        resp.command = commands::READ_DATA;
        resp.status = static_cast<uint8_t>(LFS_ERR_NAMETOOLONG);
        resp.chunkoff = header->chunkoff;
        resp.chunklen = 0;
        resp.totallen = 0;
        auto* respOm = ble_hs_mbuf_from_flat(&resp, sizeof(ReadResponse));
        ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
        systemTask.PushMessage(Pinetime::System::Messages::StopFileTransfer);
        return 0;
      }
      memcpy(filepath, header->pathstr, plen);
      filepath[plen] = 0;
      ReadResponse resp;
      os_mbuf* respOm;
      resp.command = commands::READ_DATA;
      resp.status = 0x01;
      resp.chunkoff = header->chunkoff;
      int res = fs.Stat(filepath, &info);
      if (res != 0) {
        resp.status = (int8_t) res;
        resp.chunklen = 0;
        resp.totallen = 0;
        respOm = ble_hs_mbuf_from_flat(&resp, sizeof(ReadResponse));
      } else {
        uint32_t maxChunk = MaxReadChunkBytes(connectionHandle);
        uint32_t remaining = (header->chunkoff < info.size) ? (info.size - header->chunkoff) : 0;
        resp.chunklen = std::min({header->chunksize, remaining, maxChunk});
        resp.totallen = info.size;
        FS::Lock lock(fs);
        fs.FileOpen(&f, filepath, LFS_O_RDONLY);
        fs.FileSeek(&f, header->chunkoff);
        resp.chunklen = fs.FileRead(&f, fileData, resp.chunklen);
        respOm = ble_hs_mbuf_from_flat(&resp, sizeof(ReadResponse));
        os_mbuf_append(respOm, fileData, resp.chunklen);
        fs.FileClose(&f);
      }

      ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
      break;
    }
    case commands::READ_PACING: {
      NRF_LOG_INFO("[FS_S] -> Readpacing");
      auto* header = (ReadHeader*) om->om_data;
      ReadResponse resp;
      resp.command = commands::READ_DATA;
      resp.status = 0x01;
      resp.chunkoff = header->chunkoff;
      bool fileOpened = false;
      int res = fs.Stat(filepath, &info);
      FS::Lock lock(fs);
      if (res != 0) {
        resp.status = (int8_t) res;
        resp.chunklen = 0;
        resp.totallen = 0;
      } else {
        uint32_t maxChunk = MaxReadChunkBytes(connectionHandle);
        uint32_t remaining = (header->chunkoff < info.size) ? (info.size - header->chunkoff) : 0;
        resp.chunklen = std::min({header->chunksize, remaining, maxChunk});
        resp.totallen = info.size;
        if (fs.FileOpen(&f, filepath, LFS_O_RDONLY) == 0) {
          fileOpened = true;
          fs.FileSeek(&f, header->chunkoff);
        } else {
          resp.chunklen = 0;
        }
      }
      os_mbuf* respOm;
      if (fileOpened && resp.chunklen > 0) {
        resp.chunklen = fs.FileRead(&f, fileData, resp.chunklen);
        respOm = ble_hs_mbuf_from_flat(&resp, sizeof(ReadResponse));
        os_mbuf_append(respOm, fileData, resp.chunklen);
      } else {
        resp.chunklen = 0;
        respOm = ble_hs_mbuf_from_flat(&resp, sizeof(ReadResponse));
      }
      if (fileOpened) {
        fs.FileClose(&f);
      }
      ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
      break;
    }
    case commands::WRITE: {
      NRF_LOG_INFO("[FS_S] -> Write");
      auto* header = (WriteHeader*) om->om_data;
      uint16_t plen = header->pathlen;
      WriteResponse resp {};
      resp.command = commands::WRITE_PACING;
      resp.offset = header->offset;
      resp.modTime = 0;
      if (plen > maxpathlen) {
        resp.status = static_cast<uint8_t>(LFS_ERR_NAMETOOLONG);
        resp.freespace = 0;
        auto* respOm = ble_hs_mbuf_from_flat(&resp, sizeof(WriteResponse));
        ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
        systemTask.PushMessage(Pinetime::System::Messages::StopFileTransfer);
        return 0;
      }
      memcpy(filepath, header->pathstr, plen);
      filepath[plen] = 0;
      fileSize = header->totalSize;

      int res = fs.FileOpen(&f, filepath, LFS_O_RDWR | LFS_O_CREAT);
      if (res == 0) {
        fs.FileClose(&f);
        resp.status = 0x01;
      } else {
        resp.status = static_cast<int8_t>(res);
      }
      resp.freespace = std::min(fs.getSize() - (fs.GetFSSize() * fs.getBlockSize()), fileSize - header->offset);
      auto* respOm = ble_hs_mbuf_from_flat(&resp, sizeof(WriteResponse));
      ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
      break;
    }
    case commands::WRITE_DATA: {
      NRF_LOG_INFO("[FS_S] -> WriteData");
      auto* header = (WritePacing*) om->om_data;
      WriteResponse resp {};
      resp.command = commands::WRITE_PACING;
      resp.offset = header->offset;
      int res = 0;
      {
        FS::Lock lock(fs);
        if (!(res = fs.FileOpen(&f, filepath, LFS_O_RDWR | LFS_O_CREAT))) {
          if ((res = fs.FileSeek(&f, header->offset)) >= 0) {
            res = fs.FileWrite(&f, header->data, header->dataSize);
          }
          fs.FileClose(&f);
        }
      }
      if (res < 0) {
        resp.status = static_cast<int8_t>(res);
      } else {
        resp.status = 0x01;
      }
      resp.freespace = std::min(fs.getSize() - (fs.GetFSSize() * fs.getBlockSize()), fileSize - header->offset);
      auto* respOm = ble_hs_mbuf_from_flat(&resp, sizeof(WriteResponse));
      ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
      break;
    }
    case commands::DELETE: {
      NRF_LOG_INFO("[FS_S] -> Delete");
      auto* header = (DelHeader*) om->om_data;
      uint16_t plen = header->pathlen;
      char path[maxpathlen] = {0};
      if (plen >= maxpathlen) {
        plen = maxpathlen - 1;
      }
      memcpy(path, header->pathstr, plen);
      path[plen] = 0;
      DelResponse resp {};
      resp.command = commands::DELETE_STATUS;
      int res = fs.FileDelete(path);
      resp.status = (res == 0) ? 0x01 : (int8_t) res;
      auto* respOm = ble_hs_mbuf_from_flat(&resp, sizeof(DelResponse));
      ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
      break;
    }
    case commands::MKDIR: {
      NRF_LOG_INFO("[FS_S] -> MKDir");
      auto* header = (MKDirHeader*) om->om_data;
      uint16_t plen = header->pathlen;
      char path[maxpathlen] = {0};
      if (plen >= maxpathlen) {
        plen = maxpathlen - 1;
      }
      memcpy(path, header->pathstr, plen);
      path[plen] = 0;
      MKDirResponse resp {};
      resp.command = commands::MKDIR_STATUS;
      resp.modification_time = 0;
      int res = fs.DirCreate(path);
      resp.status = (res == 0) ? 0x01 : (int8_t) res;
      auto* respOm = ble_hs_mbuf_from_flat(&resp, sizeof(MKDirResponse));
      ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
      break;
    }
    case commands::LISTDIR: {
      NRF_LOG_INFO("[FS_S] -> ListDir");
      FinishListDir();

      ListDirHeader* header = (ListDirHeader*) om->om_data;
      uint16_t plen = header->pathlen;
      char path[maxpathlen] = {0};
      if (plen >= maxpathlen) {
        plen = maxpathlen - 1;
      }
      memcpy(path, header->pathstr, plen);
      path[plen] = 0;

      ListDirResponse resp {};
      resp.command = commands::LISTDIR_ENTRY;
      resp.status = 0x01;
      resp.totalentries = 0;
      resp.entry = 0;
      resp.modification_time = 0;
      int res = fs.DirOpen(path, &listDir.dir);
      if (res != 0) {
        resp.status = (int8_t) res;
        auto* respOm = ble_hs_mbuf_from_flat(&resp, sizeof(ListDirResponse));
        ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
        systemTask.PushMessage(Pinetime::System::Messages::StopFileTransfer);
        return 0;
      }

      while (fs.DirRead(&listDir.dir, &info)) {
        listDir.totalentries++;
      }
      fs.DirRewind(&listDir.dir);
      listDir.active = true;
      listDir.finishing = false;
      listDir.connectionHandle = connectionHandle;
      listDir.entry = 0;
      SendListDirEntry(listDir.totalentries == 0);
      // Keep file-transfer wake lock until FinishListDir().
      return 0;
    }
    case commands::MOVE: {
      NRF_LOG_INFO("[FS_S] -> Move");
      MoveHeader* header = (MoveHeader*) om->om_data;
      uint16_t plen = header->OldPathLength;
      header->pathstr[plen] = 0;
      char path[maxpathlen] = {0};
      uint16_t newLen = header->NewPathLength;
      if (newLen >= maxpathlen) {
        newLen = maxpathlen - 1;
      }
      memcpy(path, &header->pathstr[plen + 1], newLen);
      path[newLen] = 0;
      MoveResponse resp {};
      resp.command = commands::MOVE_STATUS;
      int8_t res = (int8_t) fs.Rename(header->pathstr, path);
      resp.status = (res == 0) ? 1 : res;
      auto* respOm = ble_hs_mbuf_from_flat(&resp, sizeof(MoveResponse));
      ble_gattc_notify_custom(connectionHandle, transferCharacteristicHandle, respOm);
    }
    default:
      break;
  }
  NRF_LOG_INFO("[FS_S] -> done ");
  systemTask.PushMessage(Pinetime::System::Messages::StopFileTransfer);
  return 0;
}

void FSService::prepareReadDataResp(ReadHeader* header, ReadResponse* resp) {
  resp->command = commands::READ_DATA;
  resp->chunkoff = header->chunkoff;
  resp->status = 0x01;
  struct lfs_info info = {};
  int res = fs.Stat(filepath, &info);
  if (res != 0) {
    resp->status = 0x03;
    resp->chunklen = 0;
    resp->totallen = 0;
  } else {
    lfs_file f;
    resp->chunklen = std::min(header->chunksize, info.size);
    resp->totallen = info.size;
    FS::Lock lock(fs);
    fs.FileOpen(&f, filepath, LFS_O_RDONLY);
    fs.FileSeek(&f, header->chunkoff);
    resp->chunklen = fs.FileRead(&f, resp->chunk, resp->chunklen);
    fs.FileClose(&f);
  }
}
