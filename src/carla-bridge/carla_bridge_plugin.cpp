/*
 * Carla Plugin bridge code
 * Copyright (C) 2012 Filipe Coelho <falktx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the COPYING file
 */

#define CARLA_BACKEND_NO_EXPORTS
#include "carla_backend.h"
#include "carla_plugin.h"

#include <QtGui/QApplication>
#include <QtGui/QDialog>

// plugin specific
short add_plugin_ladspa(const char* filename, const char* label, const void* extra_stuff);
short add_plugin_dssi(const char* filename, const char* label, const void* extra_stuff);
short add_plugin_lv2(const char* filename, const char* label);
short add_plugin_vst(const char* filename, const char* label);

// global variables
bool close_now = false;

void plugin_bridge_show_gui(bool /*yesno*/)
{
}

void plugin_bridge_quit()
{
    close_now = true;
}

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        qWarning("%s :: bad arguments", argv[0]);
        return 1;
    }

    const char* osc_url  = argv[1];
    const char* stype    = argv[2];
    const char* filename = argv[3];
    const char* label    = argv[4];

    short id;
    PluginType itype;

    if (strcmp(stype, "LADSPA") == 0)
        itype = PLUGIN_LADSPA;
    else if (strcmp(stype, "DSSI") == 0)
        itype = PLUGIN_DSSI;
    else if (strcmp(stype, "LV2") == 0)
        itype = PLUGIN_LV2;
    else if (strcmp(stype, "VST") == 0)
        itype = PLUGIN_VST;
    else
    {
        itype = PLUGIN_NONE;
        qWarning("Invalid plugin type '%s'", stype);
        return 1;
    }

    QApplication app(argc, argv);

    set_last_error("no error");

    osc_init(label, osc_url);
    osc_send_update();

    switch (itype)
    {
    case PLUGIN_LADSPA:
        id = add_plugin_ladspa(filename, label, nullptr);
        break;
    case PLUGIN_DSSI:
        id = add_plugin_dssi(filename, label, nullptr);
        break;
    case PLUGIN_LV2:
        id = add_plugin_lv2(filename, label);
        break;
    case PLUGIN_VST:
        id = add_plugin_vst(filename, label);
        break;
    default:
        id = -1;
        break;
    }

    if (id >= 0)
    {
        CarlaPlugin* plugin = CarlaPlugins[id];

        if (plugin && plugin->id() >= 0)
        {
            // FIXME
            plugin->set_active(true, false, false);

            bool send_update = true;
            ParameterData* param_data;

            while (close_now == false)
            {
                plugin->idle_gui();
                app.processEvents();

                if (close_now) break;

                if (plugin->ain_count() > 0)
                {
                    osc_send_bridge_ains_peak(1, ains_peak[0]);
                    osc_send_bridge_ains_peak(2, ains_peak[1]);
                }

                if (close_now) break;

                if (plugin->aout_count() > 0)
                {
                    osc_send_bridge_aouts_peak(1, aouts_peak[0]);
                    osc_send_bridge_aouts_peak(2, aouts_peak[1]);
                }

                if (close_now) break;

                for (uint32_t i=0; i < plugin->param_count(); i++)
                {
                    param_data = plugin->param_data(i);

                    if (param_data->type == PARAMETER_OUTPUT && (param_data->hints & PARAMETER_IS_AUTOMABLE) > 0)
                        osc_send_control(nullptr, param_data->rindex, plugin->get_parameter_value(i));
                }

                if (close_now) break;

                if (send_update)
                {
                    send_update = false;
                    osc_send_bridge_update();
                }

                carla_msleep(50);
            }

            delete plugin;
        }
    }
    else
    {
        qWarning("Plugin failed to load, error was:\n%s", get_last_error());
        return 1;
    }

    osc_send_exiting();
    osc_close();

    return 0;
}