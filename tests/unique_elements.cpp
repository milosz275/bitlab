#include <bits/stdc++.h>

using namespace std;

/**
 * @brief Check unique elements in a file. The file should contain one record per line. The program will output the number of unique elements and the number of repeating elements.
 *
 * @param argc Number of arguments
 * @param argv Arguments
 *
 * @return 0 if successful, 1 otherwise
 */
int check_unique(int argc, char* argv[])
{
    string filename = "ips.test";
    if (argc > 1)
        filename = argv[1];

    fstream file(filename, ios::in);
    if (!file.is_open())
    {
        cout << "Could not open file " << filename << endl;
        return 1;
    }
    set<string> unique;
    string s;
    int repeating = 0;
    while (file >> s)
    {
        if (unique.find(s) != unique.end())
            repeating++;
        else
            unique.insert(s);
    }
    file.close();
    cout << "Number of unique elements: " << unique.size() << endl;
    cout << "Number of repeating elements: " << repeating << endl;
    return 0;
}

int main(int argc, char* argv[])
{
    return check_unique(argc, argv);
}
