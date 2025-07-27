#include "../Common/ConfigLoader.h"
#include "UdpClient.h"
#include <cstring>
#include <iostream>

int main (int argc, char* argv[]) {
    try {
        const auto settings = Common::SettingsLoader::load<Common::ClientSettings> ("CLIENT_SETTINGS");
        std::size_t count   = 1;
        std::string forced_imsi;

        for (int i = 1; i < argc; ++i) {
            if (std::strcmp (argv[i], "-n") == 0 && i + 1 < argc) {
                count = std::stoul (argv[++i]);
            } else {
                forced_imsi = argv[i];
            }
        }
        client::UdpClient cl (settings);
        cl.init_sockets ();

        for (std::size_t i = 0; i < count; ++i) {
            const std::string imsi = forced_imsi.empty () ? client::UdpClient::generate_imsi () : forced_imsi;

            cl.send_imsi (imsi);
            const std::string reply = cl.receive ();

            std::cout << "#" << i + 1 << "  IMSI " << imsi << " → " << reply << '\n';

            if (!forced_imsi.empty ())
                break;
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what () << std::endl;
        return 1;
    }
}
