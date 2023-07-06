#pragma once

#include <deque>

#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    // сделаем "обёртки" для всех методов поиска, 
    // чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        bool result;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_request_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> documents = search_request_.FindTopDocuments(raw_query, document_predicate);
    QueryResult query_result;

    query_result.result = (documents.empty() == false);

    if (!(requests_.size() < min_in_day_)) {
        requests_.pop_front();
    }

    requests_.push_back(query_result);

    return documents;
}