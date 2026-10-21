/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include <exception>
#include <iostream>

#include "ContainersExampleUi.h"
#include "mfd/control/CommandClient.h"
#include "mfd/io/JsonLoader.h"

int main()
{
    try
    {
        mfd::JsonLoader loader;
        const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(
            "assets/windows/containers_window.json");
        if (!loaded.window.commandTransports.udp.has_value() || !loaded.generatedTransportMap.has_value())
        {
            throw std::runtime_error("The container example requires UDP commands and its generated map.");
        }

        mfd::CommandClient client(*loaded.window.commandTransports.udp, *loaded.generatedTransportMap);
        containers_example::ContainersExampleUi ui;
        ui.Containers().MainPanel().SetVisible(true);
        ui.Containers().MainPanel().SetEnabled(true);
        ui.Containers().MainPanel().SetScrollOffset({0.0f, 0.0f});
        if (!client.SendBatch(ui.BuildBatch(), 1U))
        {
            throw std::runtime_error(client.LastError());
        }
        std::cout << "Container state sent to the API LEVEL 1 example Window.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "apilevel1_containers_client: " << exception.what() << '\n';
        return 1;
    }
}
