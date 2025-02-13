#include "gatt.h"
#include <ble/ble.h>

#include <furi.h>

#define TAG "GattChar"

#define GATT_MIN_READ_KEY_SIZE (10)

void ble_gatt_characteristic_init(
    uint16_t svc_handle,
    const BleGattCharacteristicParams* char_descriptor,
    BleGattCharacteristicInstance* char_instance) {
    furi_assert(char_descriptor);
    furi_assert(char_instance);

    // Copy the descriptor to the instance, since it may point to stack memory
    char_instance->characteristic = malloc(sizeof(BleGattCharacteristicParams));
    memcpy(
        (void*)char_instance->characteristic,
        char_descriptor,
        sizeof(BleGattCharacteristicParams));

    uint16_t char_data_size = 0;
    if(char_descriptor->data_prop_type == FlipperGattCharacteristicDataFixed) {
        char_data_size = char_descriptor->data.fixed.length;
    } else if(char_descriptor->data_prop_type == FlipperGattCharacteristicDataCallback) {
        char_descriptor->data.callback.fn(
            char_descriptor->data.callback.context, NULL, &char_data_size);
    }

    tBleStatus status = aci_gatt_add_char(
        svc_handle,
        char_descriptor->uuid_type,
        &char_descriptor->uuid,
        char_data_size,
        char_descriptor->char_properties,
        char_descriptor->security_permissions,
        char_descriptor->gatt_evt_mask,
        GATT_MIN_READ_KEY_SIZE,
        char_descriptor->is_variable,
        &char_instance->handle);
    if(status) {
        FURI_LOG_E(TAG, "Failed to add %s char: %d", char_descriptor->name, status);
    }

    char_instance->descriptor_handle = 0;
    if((status == 0) && char_descriptor->descriptor_params) {
        uint8_t const* char_data = NULL;
        const BleGattCharacteristicDescriptorParams* char_data_descriptor =
            char_descriptor->descriptor_params;
        bool release_data = char_data_descriptor->data_callback.fn(
            char_data_descriptor->data_callback.context, &char_data, &char_data_size);

        status = aci_gatt_add_char_desc(
            svc_handle,
            char_instance->handle,
            char_data_descriptor->uuid_type,
            &char_data_descriptor->uuid,
            char_data_descriptor->max_length,
            char_data_size,
            char_data,
            char_data_descriptor->security_permissions,
            char_data_descriptor->access_permissions,
            char_data_descriptor->gatt_evt_mask,
            GATT_MIN_READ_KEY_SIZE,
            char_data_descriptor->is_variable,
            &char_instance->descriptor_handle);
        if(status) {
            FURI_LOG_E(TAG, "Failed to add %s char descriptor: %d", char_descriptor->name, status);
        }
        if(release_data) {
            free((void*)char_data);
        }
    }
}

void ble_gatt_characteristic_delete(
    uint16_t svc_handle,
    BleGattCharacteristicInstance* char_instance) {
    tBleStatus status = aci_gatt_del_char(svc_handle, char_instance->handle);
    if(status) {
        FURI_LOG_E(
            TAG, "Failed to delete %s char: %d", char_instance->characteristic->name, status);
    }
    free((void*)char_instance->characteristic);
}

bool ble_gatt_characteristic_update(
    uint16_t svc_handle,
    BleGattCharacteristicInstance* char_instance,
    const void* source) {
    furi_check(char_instance);
    const BleGattCharacteristicParams* char_descriptor = char_instance->characteristic;
    FURI_LOG_D(TAG, "Updating %s char", char_descriptor->name);

    const uint8_t* char_data = NULL;
    uint16_t char_data_size = 0;
    bool release_data = false;
    if(char_descriptor->data_prop_type == FlipperGattCharacteristicDataFixed) {
        char_data = char_descriptor->data.fixed.ptr;
        if(source) {
            char_data = (uint8_t*)source;
        }
        char_data_size = char_descriptor->data.fixed.length;
    } else if(char_descriptor->data_prop_type == FlipperGattCharacteristicDataCallback) {
        const void* context = char_descriptor->data.callback.context;
        if(source) {
            context = source;
        }
        release_data = char_descriptor->data.callback.fn(context, &char_data, &char_data_size);
    }

    tBleStatus result = aci_gatt_update_char_value(
        svc_handle, char_instance->handle, 0, char_data_size, char_data);
    if(result) {
        FURI_LOG_E(TAG, "Failed updating %s characteristic: %d", char_descriptor->name, result);
    }
    if(release_data) {
        free((void*)char_data);
    }
    return result != BLE_STATUS_SUCCESS;
}

bool ble_gatt_service_add(
    uint8_t Service_UUID_Type,
    const Service_UUID_t* Service_UUID,
    uint8_t Service_Type,
    uint8_t Max_Attribute_Records,
    uint16_t* Service_Handle) {
    tBleStatus result = aci_gatt_add_service(
        Service_UUID_Type, Service_UUID, Service_Type, Max_Attribute_Records, Service_Handle);
    if(result) {
        FURI_LOG_E(TAG, "Failed to add service: %x", result);
    }

    return result == BLE_STATUS_SUCCESS;
}

bool ble_gatt_service_delete(uint16_t svc_handle) {
    tBleStatus result = aci_gatt_del_service(svc_handle);
    if(result) {
        FURI_LOG_E(TAG, "Failed to delete service: %x", result);
    }

    return result == BLE_STATUS_SUCCESS;
}
