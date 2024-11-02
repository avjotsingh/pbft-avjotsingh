#include "constants.h"

namespace Constants {
    std::vector<std::string> serverNames = { "S1", "S2", "S3", "S4", "S5", "S6", "S7" };
    std::map<std::string, std::string> serverAddresses = {
        { "S1", "localhost:50051" },
        { "S2", "localhost:50052" },
        { "S3", "localhost:50053" },
        { "S4", "localhost:50054" },
        { "S5", "localhost:50055" },
        { "S6", "localhost:50056" },
        { "S7", "localhost:50057" }
    };
    std::map<std::string, std::map<std::string, std::string>> keys = {
        {"S1", 
            {
                {"S2", "68b4c924c5296333075e4f6011222d0fc2c133c731fdcf3896bc29804e2d5a5f"},
                {"S3", "88dfea02412266119e2bf75fd99da9a482186e18d211830c4a5358b5976bdbe9"},
                {"S4", "b4445ab4519381bcf2231cc6393e2fab42f3cb6428ad78f9f6e467cbb81f9c23"},
                {"S5", "63dd85dc41f7cdd3fa5a14ade3c51ea5d07c513900d90a19c8cd9c5fa33e17cc"},
                {"S6", "3069239629220eee47962c730162267bf7d434d7cec20383153f657fa63b810c"},
                {"S7", "1201a81a41c413235623d1b62ce41c5e47f9dc3da14ca9c338245d1dfda6a7e8"}
            }
        },
        {"S2", 
            {
                {"S1", "68b4c924c5296333075e4f6011222d0fc2c133c731fdcf3896bc29804e2d5a5f"},
                {"S3", "69b2250dc04956f1e406fd31e421723b51a4f502c2e89acbe923d2c66de851c0"},
                {"S4", "5de1a5f22c8d35a2ba1297a6b9dcc984390408f9cf056c443d1700a9e22d87bd"},
                {"S5", "ceb3bc4d3811c7fe065d89f940e26f79ca4f0beffc3d971b70664e8efd0c80c9"},
                {"S6", "7ea39ef01af5d64a99de4b6cdd56835e690b5b0c03e2d8dab7bb763e76320138"},
                {"S7", "71ec58a6f680feef97f49445ebc61921b124b4f54c9b838c67dec2022874f4cd"}
            }
        },
        {"S3", 
            {
                {"S1", "88dfea02412266119e2bf75fd99da9a482186e18d211830c4a5358b5976bdbe9"},
                {"S2", "69b2250dc04956f1e406fd31e421723b51a4f502c2e89acbe923d2c66de851c0"},
                {"S4", "6eeb036b5b023e978885addee4c965b5959eda25cf3a22b509859c1940af8b55"},
                {"S5", "6e11bd422043c56f92514723e0f769cd6d031ef9c958914400252f0c4b550482"},
                {"S6", "fbf87d98ee0b2bc264bb3a7b3effd8232ca180d7090a04d77ad47493f2bddcf9"},
                {"S7", "81c67b9758ba606c93cc918b774afd34a4661a969d835bc28b421728ae8dbf52"}
            }
        },
        {"S4", 
            {
                {"S1", "b4445ab4519381bcf2231cc6393e2fab42f3cb6428ad78f9f6e467cbb81f9c23"},
                {"S2", "5de1a5f22c8d35a2ba1297a6b9dcc984390408f9cf056c443d1700a9e22d87bd"},
                {"S3", "6eeb036b5b023e978885addee4c965b5959eda25cf3a22b509859c1940af8b55"},
                {"S5", "c303714aa3b97709150b706af57eeb2edf5f3371791c434c57512033d2208e6f"},
                {"S6", "b17512042f74281b246cd1a24386f68eea2ecebce81d86d72304abebebb77e1b"},
                {"S7", "938b4db890753d12af66586fd2b7ec3a445f169791267c72825e75685242ec98"}
            }
        },
        {"S5", 
            {
                {"S1", "63dd85dc41f7cdd3fa5a14ade3c51ea5d07c513900d90a19c8cd9c5fa33e17cc"},
                {"S2", "ceb3bc4d3811c7fe065d89f940e26f79ca4f0beffc3d971b70664e8efd0c80c9"},
                {"S3", "6e11bd422043c56f92514723e0f769cd6d031ef9c958914400252f0c4b550482"},
                {"S4", "c303714aa3b97709150b706af57eeb2edf5f3371791c434c57512033d2208e6f"},
                {"S6", "fbb78ba9a477c8ed3810d49dab4412913c8e174185646ad33dac076e4158bf56"},
                {"S7", "26506f1e198195a2c377079c96a11c6ebfa83255ce45e0818c589801f1c3406e"}
            }
        },
        {"S6", 
            {
                {"S1", "3069239629220eee47962c730162267bf7d434d7cec20383153f657fa63b810c"},
                {"S2", "7ea39ef01af5d64a99de4b6cdd56835e690b5b0c03e2d8dab7bb763e76320138"},
                {"S3", "fbf87d98ee0b2bc264bb3a7b3effd8232ca180d7090a04d77ad47493f2bddcf9"},
                {"S4", "b17512042f74281b246cd1a24386f68eea2ecebce81d86d72304abebebb77e1b"},
                {"S5", "fbb78ba9a477c8ed3810d49dab4412913c8e174185646ad33dac076e4158bf56"},
                {"S7", "8074ed65a0f32bd4e0eef1c8a0563de0e8d349d692ff06004c2a6550783c941e"}
            }
        },
        {"S7", 
            {
                {"S1", "1201a81a41c413235623d1b62ce41c5e47f9dc3da14ca9c338245d1dfda6a7e8"},
                {"S2", "71ec58a6f680feef97f49445ebc61921b124b4f54c9b838c67dec2022874f4cd"},
                {"S3", "81c67b9758ba606c93cc918b774afd34a4661a969d835bc28b421728ae8dbf52"},
                {"S4", "938b4db890753d12af66586fd2b7ec3a445f169791267c72825e75685242ec98"},
                {"S5", "26506f1e198195a2c377079c96a11c6ebfa83255ce45e0818c589801f1c3406e"},
                {"S6", "8074ed65a0f32bd4e0eef1c8a0563de0e8d349d692ff06004c2a6550783c941e"}
            }
        },
    };
}