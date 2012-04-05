/*
 * JACK Backend code for Carla
 * Copyright (C) 2011-2012 Filipe Coelho <falktx@gmail.com>
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

#include "carla_jack.h"
#include "carla_plugin.h"

// Global JACK stuff
jack_client_t* carla_jack_client = nullptr;
jack_nframes_t carla_buffer_size = 512;
jack_nframes_t carla_sample_rate = 44100;

const char* carla_client_name = nullptr;

// -------------------------------------------------------------------------------------------------------------------
// Exported symbols (API)

bool carla_is_engine_running()
{
    return bool(carla_jack_client);
}

const char* get_host_client_name()
{
    //qDebug("get_host_client_name()");
    return carla_client_name;
}

uint32_t get_buffer_size()
{
    qDebug("get_buffer_size()");
    return carla_buffer_size;
}

double get_sample_rate()
{
    qDebug("get_sample_rate()");
    return carla_sample_rate;
}

double get_latency()
{
    qDebug("get_latency()");
    return double(carla_buffer_size)/carla_sample_rate*1000;
}

// End of exported symbols (API)
// -------------------------------------------------------------------------------------------------------------------

int carla_jack_bufsize_callback(jack_nframes_t new_buffer_size, void*)
{
    carla_buffer_size = new_buffer_size;

#ifdef BUILD_BRIDGE
    CarlaPlugin* plugin = CarlaPlugins[0];
    if (plugin && plugin->id() >= 0)
        plugin->buffer_size_changed(new_buffer_size);
#else
    for (unsigned short i=0; i<MAX_PLUGINS; i++)
    {
        CarlaPlugin* plugin = CarlaPlugins[i];
        if (plugin && plugin->id() >= 0)
            plugin->buffer_size_changed(new_buffer_size);
    }
#endif

    return 0;
}

int carla_jack_srate_callback(jack_nframes_t new_sample_rate, void*)
{
    carla_sample_rate = new_sample_rate;
    return 0;
}

int carla_jack_process_callback(jack_nframes_t nframes, void* arg)
{
#ifndef BUILD_BRIDGE
    if (carla_options.global_jack_client)
    {
        for (unsigned short i=0; i<MAX_PLUGINS; i++)
        {
            CarlaPlugin* plugin = CarlaPlugins[i];
            if (plugin && plugin->id() >= 0)
            {
                carla_proc_lock();
                plugin->process(nframes);
                carla_proc_unlock();
            }
        }
        return 0;
    }
#else
        Q_UNUSED(arg);
#endif

#ifdef BUILD_BRIDGE
    CarlaPlugin* plugin = CarlaPlugins[0];
#else
    CarlaPlugin* plugin = (CarlaPlugin*)arg;
#endif
    if (plugin && plugin->id() >= 0)
    {
        carla_proc_lock();
        plugin->process(nframes);
        carla_proc_unlock();
    }

    return 0;
}

void carla_jack_shutdown_callback(void*)
{
    for (unsigned short i=0; i<MAX_PLUGINS; i++)
    {
        // FIXME
        //CarlaPlugin* plugin = CarlaPlugins[i];
        //if (plugin && plugin->id() == plugin_id)
        //    plugin->jack_client = nullptr;
    }
    carla_jack_client = nullptr;
    callback_action(CALLBACK_QUIT, 0, 0, 0, 0.0);
}

// -------------------------------------------------------------------------------------------------------------------

#ifndef BUILD_BRIDGE
bool carla_jack_init(const char* client_name)
{
    carla_jack_client = jack_client_open(client_name, JackNullOption, nullptr);

    if (carla_jack_client)
    {
        carla_buffer_size = jack_get_buffer_size(carla_jack_client);
        carla_sample_rate = jack_get_sample_rate(carla_jack_client);

        jack_set_buffer_size_callback(carla_jack_client, carla_jack_bufsize_callback, nullptr);
        jack_set_sample_rate_callback(carla_jack_client, carla_jack_srate_callback, nullptr);
        jack_set_process_callback(carla_jack_client, carla_jack_process_callback, nullptr);
        jack_on_shutdown(carla_jack_client, carla_jack_shutdown_callback, nullptr);

        if (jack_activate(carla_jack_client) == 0)
        {
            // set client name, fixed for OSC usage
            char* fixed_name = strdup(jack_get_client_name(carla_jack_client));
            for (size_t i=0; i < strlen(fixed_name); i++)
            {
                if (std::isalpha(fixed_name[i]) == false && std::isdigit(fixed_name[i]) == false)
                    fixed_name[i] = '_';
            }

            carla_client_name = strdup(fixed_name);
            free((void*)fixed_name);

            return true;
        }
        else
        {
            set_last_error("Failed to activate the JACK client");
            carla_jack_client = nullptr;
        }
    }
    else
    {
        set_last_error("Failed to create new JACK client");
        carla_jack_client = nullptr;
    }

    return false;
}

bool carla_jack_close()
{
    if (carla_client_name)
        free((void*)carla_client_name);

    if (jack_deactivate(carla_jack_client) == 0)
    {
        if (jack_client_close(carla_jack_client) == 0)
        {
            carla_jack_client = nullptr;
            return true;
        }
        else
            set_last_error("Failed to close the JACK client");
    }
    else
        set_last_error("Failed to deactivate the JACK client");

    carla_jack_client = nullptr;
    return false;
}
#endif

bool carla_jack_register_plugin(CarlaPlugin* plugin, jack_client_t** client)
{
#ifndef BUILD_BRIDGE
    if (carla_options.global_jack_client)
    {
        *client = carla_jack_client;
        return true;
    }
#endif

    *client = jack_client_open(plugin->name(), JackNullOption, nullptr);

    if (*client)
    {
#ifdef BUILD_BRIDGE
        carla_buffer_size = jack_get_buffer_size(*client);
        carla_sample_rate = jack_get_sample_rate(*client);

        jack_set_buffer_size_callback(*client, carla_jack_bufsize_callback, nullptr);
        jack_set_sample_rate_callback(*client, carla_jack_srate_callback, nullptr);
        jack_set_process_callback(*client, carla_jack_process_callback, nullptr);
        jack_on_shutdown(*client, carla_jack_shutdown_callback, nullptr);
#else
        jack_set_process_callback(*client, carla_jack_process_callback, plugin);
#endif
        //if (jack_activate(*client) == 0)
        return true;
    }

    return false;
}