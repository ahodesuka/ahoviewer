#include <stdlib.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
    std::ifstream in;
    std::ofstream out;
    std::string   line;

    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " infile outfile identifier" << std::endl;
        return EXIT_FAILURE;
    }

    in.open(argv[1]);

    if (!in.is_open())
    {
        std::cerr << "Input file doesn't exist." << std::endl;
        return EXIT_FAILURE;
    }

    out.open(argv[2], std::ofstream::trunc);

    out << "char " << argv[3] << "[] =" << std::endl
        << "{" << std::endl;

    while (std::getline(in, line))
    {
        size_t pos = 0;
        while ((pos = line.find('"', pos)) != std::string::npos)
        {
            line.replace(pos, 1, "\\\"");
            pos += 2;
        }
        out << '"' << line << '"' << std::endl;
    }

    out << "};" << std::endl;

    return EXIT_SUCCESS;
}
