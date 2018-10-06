#include <gio/gio.h>
#include <gudev/gudev.h>

#include "mb-monitor.h"

struct _MbMonitor
{
    GObject parent_instance;

    GVolumeMonitor *volume_monitor;
    GUdevClient *udev_client;
    GPtrArray *devices;
};

G_DEFINE_TYPE (MbMonitor, mb_monitor, G_TYPE_OBJECT)

static gboolean
is_microbit (GMount *mount)
{
    g_autoptr(GDrive) drive = g_mount_get_drive (mount);
    g_autofree gchar *unix_device = g_drive_get_identifier (drive, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    g_autoptr(GUdevClient) client = g_udev_client_new (NULL);
    g_autoptr(GUdevDevice) device = g_udev_client_query_by_device_file (client, unix_device);
    const gchar *bus = g_udev_device_get_property (device, "ID_BUS");
    const gchar *vendor_id = g_udev_device_get_property (device, "ID_VENDOR_ID");
    const gchar *model_id = g_udev_device_get_property (device, "ID_MODEL_ID");
    return g_strcmp0 (bus, "usb") == 0 && g_strcmp0 (vendor_id, "0d28") == 0 && g_strcmp0 (model_id, "0204") == 0;
}

static void
mount_added_cb (MbMonitor *self, GMount *mount)
{
    if (!is_microbit (mount))
        return;

    MbDevice *device = mb_device_new (mount);
    g_ptr_array_add (self->devices, device);
    g_printerr ("+microbit\n");
}

static MbDevice *
find_device (MbMonitor *self, GMount *mount)
{
    for (int i = 0; i < self->devices->len; i++) {
        MbDevice *device = g_ptr_array_index (self->devices, i);
        if (mb_device_get_mount (device) == mount)
            return device;
    }

    return NULL;
}

static void
mount_removed_cb (MbMonitor *self, GMount *mount)
{
    MbDevice *device = find_device (self, mount);
    if (device != NULL)
        g_printerr ("-microbit\n");
}

void
mb_monitor_init (MbMonitor *self)
{
    self->volume_monitor = g_volume_monitor_get ();
    g_signal_connect_object (self->volume_monitor, "mount-added", G_CALLBACK (mount_added_cb), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->volume_monitor, "mount-removed", G_CALLBACK (mount_removed_cb), self, G_CONNECT_SWAPPED);

    self->udev_client = g_udev_client_new (NULL);

    self->devices = g_ptr_array_new_with_free_func (g_object_unref);

    GList *mounts = g_volume_monitor_get_mounts (self->volume_monitor);
    for (GList *link = mounts; link != NULL; link = link->next) {
        GMount *mount = link->data;
        mount_added_cb (self, mount);
    }
    g_list_free_full (mounts, g_object_unref);
}

static void
mb_monitor_dispose (GObject *object)
{
    MbMonitor *self = MB_MONITOR (object);

    g_clear_object (&self->volume_monitor);
    g_clear_object (&self->udev_client);
    g_clear_pointer (&self->devices, g_ptr_array_unref);

    G_OBJECT_CLASS (mb_monitor_parent_class)->dispose (object);
}

void
mb_monitor_class_init (MbMonitorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = mb_monitor_dispose;
}

MbMonitor *
mb_monitor_new (void)
{
    return g_object_new (mb_monitor_get_type (), NULL);
}

GPtrArray *
mb_monitor_get_devices (MbMonitor *monitor)
{
    g_return_val_if_fail (MB_IS_MONITOR (monitor), NULL);
    return monitor->devices;
}
