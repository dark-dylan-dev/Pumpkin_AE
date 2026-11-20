#include <Benchmark.hpp>
#include <iostream>
#include <AE/Codec.hpp>

int main() {
    int choice = 1;

    std::println("==========Benchmark==========");
    std::println("\nChoose your configuration:");
    std::println("1. 10 Inventories, 10 Items per inventory");
    std::println("2. 100 Inventories, 100 Items per inventory");
    std::println("3. 1000 Inventories, 1000 Items per inventory");

    std::print("\nYour choice: ");
    std::cin >> choice;

    int nbInventories = 0;
    int nbItemsPerInventory = 0;

    switch (choice)
    {
        case 1:
            nbInventories = 10;
            nbItemsPerInventory = 10;
            break;

        case 2:
            nbInventories = 100;
            nbItemsPerInventory = 100;
            break;

        case 3:
            nbInventories = 1000;
            nbItemsPerInventory = 1000;
            break;

        default:
            std::println("Invalid configuration.");
            return 1;
    }

    std::println(
        "\nSelected: {} inventories, {} items per inventory ({} total items).\n",
        nbInventories,
        nbItemsPerInventory,
        nbInventories * nbItemsPerInventory
    );

    RunBenchmark(nbInventories, nbItemsPerInventory);

    std::println("\n");
#if defined(_WIN32)
    std::system("pause");
#endif

}

