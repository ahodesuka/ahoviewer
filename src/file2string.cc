#include <cstdlib>
#include <fstream>
#include <iostream>

int main(int argc, char **argv)
{
    std::ifstream in(argv[1], std::ifstream::binary | std::ifstream::ate);
    std::ofstream out(argv[2], std::ofstream::trunc);

    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " infile outfile identifier" << std::endl;
        return EXIT_FAILURE;
    }

    if (!in.is_open())
    {
        std::cerr << "Input file doesn't exist." << std::endl;
        return EXIT_FAILURE;
    }

    out << "unsigned long " << argv[3] << "_size = " << in.tellg() << ";" << std::endl;
    out << "unsigned char " << argv[3] << "[] =" << std::endl
        << "{" << std::endl;

    char inchar;
    size_t newline = 0;
    in.seekg(0, in.beg);

    while (in >> std::noskipws >> inchar)
    {
        if (newline >= 30)
        {
            newline = 0;
            out << std::endl;
        }

        out << static_cast<unsigned>(inchar) << ",";
        ++newline;
    }

    out.seekp(-1, out.cur);
    out << std::endl << "};" << std::endl;

    return EXIT_SUCCESS;
}
