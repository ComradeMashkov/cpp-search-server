#include "remove_duplicates.h"

#include <iostream>

 void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> ids_to_remove;
    std::map<std::set<std::string>, int> unique_words_ids;
    
    for (const int document_id : search_server) {
        std::map<std::string, double> word_frequencies = search_server.GetWordFrequencies(document_id);
        std::set<std::string> unique_words;
        
        for (const auto [word, _] : word_frequencies) {
            unique_words.insert(word);
        }
        
        if (unique_words_ids.count(unique_words)) {
            ids_to_remove.insert(document_id);
        }
        else {
            unique_words_ids.insert(std::make_pair(unique_words, document_id));
        }
    }
    
    for (const int id : ids_to_remove) {
        using namespace std::string_literals;
        std::cout << "Found duplicate document id "s << id << std::endl;
        search_server.RemoveDocument(id);
    }
 }

