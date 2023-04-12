#include <algorithm>
#include <iostream>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,                     // Статус по умолчанию
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    // Выводит топ-MAX_RESULT_DOCUMENT_COUNT документов в порядке убывания релевантности
    // Если релевантности равны, то в порядке убывания рейтинга
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        
        return matched_documents;
    }
    
    // Для ранжирования документов с определенным статусом
     vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status] (int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
    }
    
    // Стандартный вызов функции
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;

        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }

        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Вычисление TF-IDF
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    // Вспомогательная функция для поиска всех документов
    // Необходима для работы FindTopDocuments()
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("in"s);
        assert(found_docs.size() == 1);

        const Document& doc0 = found_docs[0];
        assert(doc0.id == doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        assert(server.FindTopDocuments("in"s).empty());
    }
}

// Тест проверяет, что добавленный документ должен находиться по поисковому запросу, 
// который содержит слова из документа
void TestAddedDocumentInQuery() {
    SearchServer server;
    server.AddDocument(0, "вкусный квас продается на площади"s, DocumentStatus::ACTUAL, {5, -7, 2});
    server.AddDocument(1, "прохладный напиток на площади"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "кошара подкрался незаметно"s, DocumentStatus::BANNED, {-10, -10, -10, -10});
    server.AddDocument(3, "электричка и квас"s, DocumentStatus::IRRELEVANT, {0, 1});

    // Смотрим количество документов по слову "квас" и id первого
    {
        const auto found_docs = server.FindTopDocuments("квас"s);
        assert(found_docs.size() == 2);
        assert(found_docs[0].id == 0);
    }

    // Смотрим количество документов по слову "кошара" и id второго
    {
        const auto found_docs = server.FindTopDocuments("кошара"s);
        assert(found_docs.size() == 1);
        assert(found_docs[0].id == 2);
    }
}

// Тест проверяет, что стоп-слова исключаются из документов
void TestStopWordsRemovedFromDocuments() {
    SearchServer server;
    server.AddDocument(0, "стоп слово и середина"s, DocumentStatus::ACTUAL, {2, 0, 1});
    server.AddDocument(1, "и стоп слово начало"s, DocumentStatus::REMOVED, {0});
    server.AddDocument(2, "стоп слово конец и"s, DocumentStatus::IRRELEVANT, {-1, -1});
    server.AddDocument(3, "и тут несколько в стоп слов на"s, DocumentStatus::ACTUAL, {2, 2, 2});

    // Убираем одно стоп-слово
    {
        server.SetStopWords("и"s);
        const auto found_docs = server.FindTopDocuments("и"s);
        assert(found_docs.size() == 0);
    }

    // Убираем несколько стоп-слов
    {
        server.SetStopWords("и в на"s);
        const auto found_docs = server.FindTopDocuments("на"s);
        assert(found_docs.size() == 0);
    }
}

// Тест проверяет, что минус-слова не включаютсч в результаты поиска
void TestMinusWordAreNotInSearchResult() {

}

// Тест проверяет, что при матчинге документа по поисковому запросу
// должны быть возвращены все слова из поискового запроса, присутствующие в документе
// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов
void TestDocumentsMatching() {

}

// Тест проверяет, что возвращаемые при поиске документов результаты отсортированы
// в порядке убывания релевантности
void TestRelevanceDescendingSorting() {

}

// Тест проверяет, что рейтинг добавленного документа равен среднему арифметическому
// оценок документа
void TestCorrectRatingComputing() {

}

// Тест проверяет, что результаты поиска фильтруются с использованием пользовательского предиката
void TestSearchResultsFilteredWithPredicates() {

}

// Тест проверяет, что работает поиск документов с заданным статусом
void TestSearchDocumentsWithStatus() {

}

// Тест проверяет, что релевантность найденных документов вычисляется корректно
void TestCorrectRelevanceComputing() {

}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}