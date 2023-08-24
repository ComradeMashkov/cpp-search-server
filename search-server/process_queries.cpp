#include "process_queries.h"

#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> buff(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), buff.begin(), [&search_server] (std::string query) {
        return search_server.FindTopDocuments(query); 
    });
    return buff;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    return std::transform_reduce(std::execution::par, queries.begin(), queries.end(), std::vector<Document>{}, [] (std::vector<Document> destination, const std::vector<Document>& departure) {
        destination.insert(destination.end(), departure.begin(), departure.end()); 
        return destination; 
    }, [&search_server] (const std::string& query) {
        return search_server.FindTopDocuments(query); 
    });
}