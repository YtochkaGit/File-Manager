#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include <conio.h>
#include <algorithm>
#include <windows.h>

namespace fs = std::filesystem;

HANDLE buffers[2];
int activeBuffer = 0;

void bufferload(const std::ostringstream& screen) {
    HANDLE backBuffer = buffers[1 - activeBuffer];

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(backBuffer, &csbi);

    DWORD size = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written;

    FillConsoleOutputCharacterA(
        backBuffer,
        ' ',
        size,
        { 0, 0 },
        &written);

    FillConsoleOutputAttribute(
        backBuffer,
        csbi.wAttributes,
        size,
        { 0, 0 },
        &written);

    SetConsoleCursorPosition(backBuffer, { 0, 0 });

    std::string frame = screen.str();

    WriteConsoleA(
        backBuffer,
        frame.c_str(),
        static_cast<DWORD>(frame.size()),
        &written,
        NULL);

    activeBuffer = 1 - activeBuffer;
    SetConsoleActiveScreenBuffer(buffers[activeBuffer]);
}

struct MarkedItem
{
    fs::path path;
    bool exists = false;
};

fs::path currentDir = fs::current_path();
std::vector<fs::directory_entry> items;

int cursorPos = 0;
int listshift = 0;
int listcount = 18;
int listfill = 0;
MarkedItem marked;

void loadDirectory()
{
    items.clear();

    for (const auto& entry : fs::directory_iterator(currentDir))
    {
        items.push_back(entry);
    }

    if (cursorPos >= listcount)
        cursorPos = 0;

    if (18 > items.size()) {
        listcount = items.size();
        listfill = 18 - listcount;
    }
    else {
        listcount = 18;
        listfill = 0;
    }

    std::sort(items.begin(), items.end(),
        [](const auto& a, const auto& b)
        {
            if (a.is_directory() != b.is_directory())
                return a.is_directory() > b.is_directory();

            return false;
        });
    if (items.empty() || cursorPos + listshift >= items.size())
    {
        cursorPos = 0;
        listshift = 0;
    }
}

void printDirectory()
{
    std::ostringstream screen;

    screen << "Current directory:\n";
    screen << currentDir.string() << "\n";
    screen << "Number of items: " << items.size() << "\n\n";

    for (int i = 0; i < (items.size() > listcount ? listcount : items.size()); i++)
    {
        if (i == cursorPos)
            screen << "> ";
        else
            screen << "  ";

        if (items[i + listshift].is_directory())
            screen << "[DIR] ";
        else
            screen << "      ";

        screen << std::left << std::setw(60) << items[i + listshift].path().filename().string() << "| ";

        if (items[i + listshift].is_regular_file())
            screen << std::left << std::setw(5) << items[i + listshift].file_size() << " Bytes";

        if (marked.exists &&
            items[i + listshift].path() == marked.path)
        {
            screen << "  [MARK]";
        }

        screen << "\n"; 
    }

    screen << "\n";
    for (int i = 0; i < listfill; i++)
        screen << std::endl;
    screen << "UP/DOWN - move\n";
    screen << "ENTER - open folder\n";
    screen << "BACKSPACE - parent folder\n";
    screen << std::left << std::setw(20) << "D - delete";
    screen << std::left << std::setw(20) << "M - mark" << std::endl;
    screen << std::left << std::setw(20) << "C - copy marked";
    screen << std::left << std::setw(20) << "X - cut marked" << std::endl;
    screen << std::left << std::setw(20) << "F - view file";
    screen << std::left << std::setw(20) << "ESC - exit" << std::endl;

    if (marked.exists)
        screen << "Marked item: " << marked.path.filename().string();

    bufferload(screen);
}

void deleteCurrent()
{
    if (items.empty())
        return;

    std::ostringstream screen;

    screen << "Delete " << items[cursorPos + listshift].path().filename().string() << "? [Y/N]\n";
     
    bufferload(screen);

    int key = _getch();

    if (key == 'y' || key == 'Y') {
        fs::remove_all(items[cursorPos + listshift].path());

        if (marked.exists && marked.path == items[cursorPos + listshift].path())
        {
            marked.exists = false;
        }

        loadDirectory();
    }
    else
        return;
}

void markCurrent()
{
    if (items.empty())
        return;

    marked.path = items[cursorPos + listshift].path();
    marked.exists = true;
}

void unmarkCurrent()
{
    if (items.empty())
        return;

    marked.path.clear();
    marked.exists = false;
}

void copyMarked()
{
    if (!marked.exists)
        return;

    fs::path destination =
        currentDir / marked.path.filename();

    try
    {
        if (fs::is_directory(marked.path))
        {
            fs::copy(
                marked.path,
                destination,
                fs::copy_options::recursive
            );
        }
        else
        {
            fs::copy_file(
                marked.path,
                destination,
                fs::copy_options::overwrite_existing
            );
        }
    }
    catch (...)
    {
    }

    loadDirectory();
}

void cutMarked()
{
    if (!marked.exists)
        return;

    fs::path destination =
        currentDir / marked.path.filename();

    try
    {
        fs::rename(marked.path, destination);
        marked.exists = false;
    }
    catch (...)
    {
    }

    loadDirectory();
}

void viewFile()
{
    if (items.empty())
        return;

    if (items[cursorPos + listshift].is_directory())
        return;

    std::ostringstream screen;

    std::ifstream file(items[cursorPos + listshift].path());

    std::string line;

    while (std::getline(file, line))
    {
        screen << line << "\n";
    }

    screen << "\n\nPress any key...";

    bufferload(screen);

    _getch();
}

int main()
{
    buffers[0] = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL);

    buffers[1] = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL);

    SetConsoleActiveScreenBuffer(buffers[0]);

    loadDirectory();

    while (true)
    {
        printDirectory();

        int key = _getch();

        if (key == 224)
        {
            key = _getch();

            if (key == 72)
            {
                if (cursorPos > 0)
                    cursorPos--;
                else if (listshift > 0)
                    listshift--;
            }
            else if (key == 80)
            {
                if (cursorPos < listcount - 1)
                    cursorPos++;
                else if (cursorPos + listshift < items.size() - 1)
                    listshift++;
            }
        }
        else if (key == 13)
        {
            if (!items.empty() &&
                items[cursorPos + listshift].is_directory())
            {
                currentDir = items[cursorPos + listshift].path();
                cursorPos = 0;
                listshift = 0;
                loadDirectory();
            }
        }
        else if (key == 8)
        {
            if (currentDir.has_parent_path())
            {
                currentDir = currentDir.parent_path();
                cursorPos = 0;
                listshift = 0;
                loadDirectory();
            }
        }
        else if (key == 'd' || key == 'D')
        {
            deleteCurrent();
        }
        else if ((key == 'm' || key == 'M') && !items.empty())
        {
            if (!marked.exists)
                markCurrent();
            else if (marked.path == items[cursorPos + listshift].path())
                unmarkCurrent();
            else 
                markCurrent();
        }
        else if (key == 'c' || key == 'C')
        {
            if (marked.exists)
            {
                copyMarked();
            }
        }
        else if (key == 'x' || key == 'X')
        {
            if (marked.exists)
            {
                cutMarked();
            }
        }
        else if (key == 'f' || key == 'F')
        {
            viewFile();
        }
        else if (key == 27)
        {
            break;
        }
    }
    CloseHandle(buffers[0]);
    CloseHandle(buffers[1]);
    return 0;
}