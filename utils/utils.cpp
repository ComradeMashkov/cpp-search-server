#include <iostream>
#include <set>
#include <vector>
#include <algorithm>

using namespace std;

template <typename It>
void PrintRange(It range_begin, It range_end) {
    for (auto it = range_begin; it != range_end; ++it) {
        cout << *it << " "s;
    }
    cout << endl;

    /* while (range_begin != range_end) {
        cout << *range_begin << " "s;
        ++range_begin;
    }
    cout << endl; */
}

template <typename Container, typename Element>
void FindAndPrint(const Container& container, const Element& element) {
    auto it = find(container.begin(), container.end(), element);
    PrintRange(container.begin(), it);
    PrintRange(it, container.end());
}

int main() {
    set<int> test = {1, 1, 1, 2, 3, 4, 5, 5};
    cout << "Test1"s << endl;
    FindAndPrint(test, 3);
    cout << "Test2"s << endl;
    FindAndPrint(test, 0); // элемента 0 нет в контейнере
    cout << "End of tests"s << endl;
    return 0;
}