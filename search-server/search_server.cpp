#include "search_server.h"

#include <cmath>
#include <algorithm>
#include <numeric>

using namespace std::string_literals;

SearchServer::SearchServer(std::string_view stop_words_text) 
    : SearchServer(SplitIntoWords(stop_words_text)) {        
 } 

SearchServer::SearchServer(std::string stop_words_text) 
    : SearchServer(SplitIntoWords(stop_words_text)) {        
 } 

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("ID документа не должен быть отрицательным"s);
    }
    if (documents_.count(document_id)) {
        throw std::invalid_argument("Такой ID документа уже существует"s);
    }
    
    auto [document_id_emplaced, document_data_emplaced] = documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, std::string(document)});
    
    document_ids_.push_back(document_id);

    const auto words = SplitIntoWordsNoStop(document_id_emplaced->second.string_data);
    const double inv_word_count = 1.0 / words.size();
    
    for (std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_to_document_freqs_ids_[document_id][word] += inv_word_count;
    }
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

MatchTuple SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

MatchTuple SearchServer::MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const {
    if ((document_id < 0) || (documents_.count(document_id) == 0)) {
        throw std::invalid_argument("Несуществующий ID документа"s);
    }
    
    const Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;
  
    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {{}, documents_.at(document_id).status};
        }
    }
    
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
                continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    
    return std::tuple<std::vector<std::string_view>, DocumentStatus>{matched_words, documents_.at(document_id).status};

}

MatchTuple SearchServer::MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words(query.plus_words.size());
    
    const auto check_if_word_exists = [this, document_id] (std::string_view word) {
        const auto tmp = word_to_document_freqs_.find(word);
        return tmp != word_to_document_freqs_.end() && tmp->second.count(document_id);
    };
 
    if (std::any_of(std::execution::par, 
                    query.minus_words.begin(), 
                    query.minus_words.end(), 
                    check_if_word_exists)) {
                        return {{}, documents_.at(document_id).status};
    }
    
    auto copy_end = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(),  matched_words.begin(), check_if_word_exists);
    
    std::sort(matched_words.begin(), copy_end);
    copy_end = std::unique(matched_words.begin(), copy_end);
    matched_words.erase(copy_end, matched_words.end());
    
    return std::tuple<std::vector<std::string_view>, DocumentStatus>{matched_words, documents_.at(document_id).status};
}

std::vector<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> dummy;
    return (word_to_document_freqs_ids_.count(document_id) ? word_to_document_freqs_ids_.at(document_id) : dummy);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    auto found_document = std::find(document_ids_.begin(), document_ids_.end(), document_id);
    if (found_document != document_ids_.end()) {
        document_ids_.erase(found_document);
        documents_.erase(document_id);
        std::for_each(word_to_document_freqs_.begin(), word_to_document_freqs_.end(), [document_id] (auto& tmp) { tmp.second.erase(document_id); });
    }
    return;
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
    auto found_document = std::find(std::execution::par, document_ids_.begin(), document_ids_.end(), document_id);
    if (found_document != document_ids_.end()) {
        document_ids_.erase(found_document);
        documents_.erase(document_id);
    }
    return;
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

std::string SearchServer::ShieldString(const std::string& str) const {
    std::string result = ""s;
    for (char chr : str) {
        if (chr >= '\0' && chr < ' ') {
            continue;
        }
        result += chr;
    }
    return result;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument(ShieldString("В слове \""s + std::string(word) + "\" присутствуют недопустимые символы"s));
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

 int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    bool is_minus = false;
    
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }

    if (!IsValidWord(text)) {
        throw std::invalid_argument(ShieldString("В слове \""s + std::string(text) + "\" запроса содержатся недопустимые символы"s));
    }

    if (text[0] == '-' || text.size() == 0) {
        throw std::invalid_argument("В поисковом запросе присутствуют два знака минуса подряд и/или отсутствуют слова после знака минус"s);
    }
    
    const QueryWord query_word = {text, is_minus, IsStopWord(text)};
    return query_word;
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    Query query;
    
    for (std::string_view word : SplitIntoWords(text)) {
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    
    std::sort(std::execution::par, query.minus_words.begin(), query.minus_words.end());
    std::sort(std::execution::par, query.plus_words.begin(), query.plus_words.end());
        
    query.minus_words.erase(std::unique(query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());
    query.plus_words.erase(std::unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());
    
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}