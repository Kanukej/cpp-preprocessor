#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;


path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool check_file(path& file_path, const string& in_file, const path& dir) {
    file_path = dir / in_file;
    if (filesystem::exists(file_path)) {
        return true;
    }
    return false;
}

bool find_file(path& file_path, const string& in_file, const vector<path>& include_directories) {
    for (const auto& d : include_directories) {
        if (check_file(file_path, in_file, d)) {
            return true;
        }
    }
    return false;
}

bool PrintFileNotFoundError(const string& file_name, const string& from, const size_t line_number) {
    cout << "unknown include file " << file_name << " at file " << from << " at line " << line_number << endl;
    return false;
}

bool Preprocess(const path& in_file, ostream& os, const vector<path>& include_directories) {
    static regex quote_include(R"(\s*#\s*include\s*\"([^"]*)\"\s*)");
    static regex corner_include(R"(\s*#\s*include\s*<([^>]*)>\s*)");
    
    ifstream in(in_file);
    if (!in) {
        return false;
    }
    
    string text;
    size_t line_number = 0;
    path file_path;
    while (getline(in, text)) {
        smatch m;
        ++line_number;
        bool need_check = false;
        if (regex_match(text, m, quote_include)) {
            need_check = !check_file(file_path, m.str(1), in_file.parent_path());
        } else if (regex_match(text, m, corner_include)) {
            need_check = true;
        } else {
            os << text << endl;
            continue;
        }
        if (need_check && !find_file(file_path, m.str(1), include_directories)) {
            return PrintFileNotFoundError(m.str(1), in_file.string(), line_number);
        }
        Preprocess(file_path, os, include_directories);
    }
    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    if (!filesystem::exists(in_file)) {
        return false;
    }
    ofstream out(out_file);
    if (!out) {
        return false;
    }
    return Preprocess(in_file, out, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;
    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
