#pragma once

#include <vector>
#include <iostream>
#include <algorithm>

template <typename Iterator>
class IteratorRange {
public:
    explicit IteratorRange(Iterator begin, Iterator end)
        : first_(begin)
        , last_(end)
        , size_(distance(first_, last_)) {
        }

    Iterator begin() const {
        return first_;
    }

    Iterator end() const {
        return last_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator first_, last_;
    size_t size_;
};

std::ostream& operator<<(std::ostream& out, const Document& document) {
    using namespace std::string_literals;
    out << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s;
    return out;
}

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t i = distance(begin, end); i > 0;) {
            const size_t current_page_size = std::min(page_size, i);
            const Iterator current_page_end = next(begin, current_page_size);
            pages_.push_back(IteratorRange<Iterator>(begin, current_page_end));

            i -= current_page_size;
            begin = current_page_end;
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}