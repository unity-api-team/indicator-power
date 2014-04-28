/*
 * Copyright 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Yuan-Chen Cheng <yc.cheng@canonical.com>
 */

#include "ib-brightness-powerd-control.h"

static gboolean getBrightnessParams(GDBusProxy* powerd_proxy, int *dim, int *min,
    int *max, int *dflt, gboolean *ab_supported);

GDBusProxy*
powerd_get_proxy(brightness_params_t *params)
{
    GError *error = NULL;
    gboolean ret;

    g_return_val_if_fail (params != NULL, NULL);

    GDBusProxy* powerd_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                G_DBUS_PROXY_FLAGS_NONE,
                NULL,
                "com.canonical.powerd",
                "/com/canonical/powerd",
                "com.canonical.powerd",
                NULL,
                &error);

    if (error != NULL)
    {
        g_debug ("could not connect to powerd: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    ret = getBrightnessParams(powerd_proxy, &(params->dim), &(params->min),
        &(params->max), &(params->dflt), &(params->ab_supported));

    if (! ret)
    {
        g_debug ("can't get brightness parameters from powerd");
        g_object_unref (powerd_proxy);
        return NULL;
    }
    
    return powerd_proxy;
}


static gboolean
getBrightnessParams(GDBusProxy* powerd_proxy, int *dim, int *min, int *max, int *dflt, gboolean *ab_supported)
{
    GVariant *ret = NULL;
    GError *error = NULL;

    ret = g_dbus_proxy_call_sync(powerd_proxy,
            "getBrightnessParams",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            400, NULL, &error); // timeout: 400 ms
    if (!ret)
    {
        if (error != NULL)
        {
            if (!g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
            {
                g_warning("getBrightnessParams from powerd failed: %s", error->message);
            }
            g_error_free(error);
        }
        return FALSE;
    }

    g_variant_get(ret, "((iiiib))", dim, min, max, dflt, ab_supported);
    g_variant_unref(ret);
    return TRUE;
}

static gboolean setUserBrightness(GDBusProxy* powerd_proxy, GCancellable *gcancel, int brightness)
{
    GVariant *ret = NULL;
    GError *error = NULL;

    ret = g_dbus_proxy_call_sync(powerd_proxy,
            "setUserBrightness",
            g_variant_new("(i)", brightness),
            G_DBUS_CALL_FLAGS_NONE,
            -1, gcancel, &error);
    if (!ret) {
        g_warning("setUserBrightness via powerd failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    } else {
        g_variant_unref(ret);
        return TRUE;
    }
}

struct _IbBrightnessPowerdControl
{
    GDBusProxy *powerd_proxy;
    GCancellable *gcancel;

    int dim;
    int min;
    int max;
    int dflt; // defalut value
    gboolean ab_supported;

    int current;
};

IbBrightnessPowerdControl*
ib_brightness_powerd_control_new (GDBusProxy* powerd_proxy, brightness_params_t params)
{
    IbBrightnessPowerdControl *control;

    control = g_new0 (IbBrightnessPowerdControl, 1);
    control->powerd_proxy = powerd_proxy;
    control->gcancel = g_cancellable_new();

    control->dim = params.dim;
    control->min = params.min;
    control->max = params.max;
    control->dflt = params.dflt;
    control->ab_supported = params.ab_supported;

    // XXX: set the brightness value is the only way to sync the brightness value with
    // powerd, and we should set the user prefered / last set brightness value upon startup.
    // Before we have code to store last set brightness value or other mechanism, we set
    // it to default brightness that powerd proposed.
    ib_brightness_powerd_control_set_value(control, control->dflt);

    return control;
}

void
ib_brightness_powerd_control_set_value (IbBrightnessPowerdControl* self, gint value)
{
    gboolean ret;

    value = CLAMP(value, self->min, self->max);
    ret = setUserBrightness(self->powerd_proxy, self->gcancel, value);
    if (ret)
    {
        self->current = value;
    }
}

gint
ib_brightness_powerd_control_get_value (IbBrightnessPowerdControl* self)
{
    return self->current;
}

gint
ib_brightness_powerd_control_get_max_value (IbBrightnessPowerdControl* self)
{
    return self->max;
}

void
ib_brightness_powerd_control_free (IbBrightnessPowerdControl *self)
{
    g_cancellable_cancel (self->gcancel);
    g_object_unref (self->gcancel);
    g_object_unref (self->powerd_proxy);
    g_free (self);
}
