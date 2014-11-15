#ifndef _NATURALSORT_H_
#define _NATURALSORT_H_

#include <cctype>
#include <cstdlib>
#include <functional>

namespace AhoViewer
{
    class NaturalSort : public std::binary_function<std::string, std::string, bool>
    {
    public:
        bool operator()(const std::string &a, const std::string &b)
        {
            return compare_natural(a.c_str(), b.c_str());
        }
    private:
        bool compare_natural(const char *a, const char *b)
        {
            // End of string
            if (!a || !b) return a ? true : false;

            // Both are numbers, compare them as such
            if (std::isdigit(*a) && std::isdigit(*b))
            {
                char *aAfter, *bAfter;
                unsigned long aL = strtoul(a, &aAfter, 10),
                              bL = strtoul(b, &bAfter, 10);

                if (aL != bL) return aL < bL;

                return compare_natural(aAfter, bAfter);
            }

            // One of them is a number
            if (std::isdigit(*a) || std::isdigit(*b))
                return std::isdigit(*a) ? true : false;

            // Both of them are not numbers
            while (*a && *b)
            {
                // Both letters were the same, are they numbers?
                if (std::isdigit(*a) || std::isdigit(*b))
                    return compare_natural(a, b);

                // Both are letters but different letters
                if (std::tolower(*a) != std::tolower(*b))
                    return std::tolower(*a) < std::tolower(*b);

                ++a;
                ++b;
            }

            return *a ? true : false;
        }
    };
}

#endif /* _NATURALSORT_H_ */
