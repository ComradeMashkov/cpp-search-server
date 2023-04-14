#include <algorithm>
#include <iostream>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MIN_TOLERANCE_COMPARISON = 1e-6;

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
                 if (abs(lhs.relevance - rhs.relevance) < MIN_TOLERANCE_COMPARISON) {
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

// -------- Перегрузка операторов вывода для контейнеров -------

// Перегруженный оператор вывода для пар словаря
template <typename First, typename Second>
ostream& operator<<(ostream& out, const pair<First, Second>& p) {
    return out << p.first << ": "s << p.second;
}

// Универсальная функция вывода для контейнера
template <typename Container>
ostream& Print(ostream& out, const Container& container) {
    bool is_first = true;

    for (const auto& element : container) {
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
    
    return out;
}

// Перегруженный оператор вывода для вектора
template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

// Перегруженный оператор вывода для множества
template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

// Перегруженный оператор вывода для словаря
template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

// -------- Макросы фреймворка собственного тестирования -------

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line, const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template<typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file, const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

// Функция и макрос для запуска тестов
template<typename Func>
void RunTestImpl(const Func& f, const string& func) {
    f();
    cerr << func << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

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
        ASSERT_EQUAL(found_docs.size(), 1u);

        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что добавленный документ должен находиться по поисковому запросу, 
// который содержит слова из документа
void TestAddedDocumentInQuery() {
    SearchServer server;
    server.AddDocument(0, "вкусный квас продается на площади"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "прохладный напиток на площади"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "кошара подкрался незаметно"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(3, "электричка и квас полный расколбас"s, DocumentStatus::ACTUAL, {1});

    // Смотрим количество документов по слову "квас" и id первого
    {
        const auto found_docs = server.FindTopDocuments("квас"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
        ASSERT_EQUAL(found_docs[0].id, 0);
    }

    // Смотрим количество документов по слову "кошара" и id второго
    {
        const auto found_docs = server.FindTopDocuments("кошара"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 2);
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
        ASSERT_HINT(found_docs.empty(), "Stop words must be excluded from documents"s);
    }

    // Убираем несколько стоп-слов
    {
        server.SetStopWords("и в на"s);
        const auto found_docs = server.FindTopDocuments("на"s);
        ASSERT_HINT(found_docs.empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что минус-слова не включаются в результаты поиска
void TestMinusWordAreNotInSearchResult() {
    SearchServer server;
    server.AddDocument(0, "вкусный квас продается на площади"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "прохладный напиток на площади"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "кошара подкрался незаметно"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(3, "электричка и квас полный расколбас"s, DocumentStatus::ACTUAL, {1});

    // Убираем из поиска одно минус-слово
    {
        const auto found_docs = server.FindTopDocuments("вкусный -квас продается на площади"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
    }

    // Убираем из поиска несколько минус-слов
    {
        const auto found_docs = server.FindTopDocuments("прохладный -квас продается -кошара"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
    }
}

// Тест проверяет, что при матчинге документа по поисковому запросу
// должны быть возвращены все слова из поискового запроса, присутствующие в документе
// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов
void TestDocumentsMatching() {
    SearchServer server;
    server.AddDocument(0, "дурацкая выставка смешных котов"s, DocumentStatus::ACTUAL, {0, 1, 2, 3, 4});
    set<string> matched_words = {"выставка"s, "котов"s};

    // Проверяем, что запрос "интересная выставка красивых котов" пересекается
    // с документом "дурацкая выставка смешных котов" по словам "выставка", "котов"
    {
        // Создаем кортеж с запросом и распаковываем его
        tuple<vector<string>, DocumentStatus> match_document = server.MatchDocument("интересная выставка красивых котов"s, 0);
        const auto [words, _] = match_document;

        ASSERT_EQUAL(words.size(), 2u);
        for (const auto& word : words) {
            ASSERT_HINT(matched_words.count(word) != 0, "Documents must match"s);
        }
    }

    // Проверяем, что наличие хотя бы одного минус-слова сразу исключает мэтч запроса с документом
    {
        tuple<vector<string>, DocumentStatus> match_document = server.MatchDocument("интересная -выставка красивых котов"s, 0);
        const auto [words, _] = match_document;
        ASSERT_HINT(words.empty(), "Minus words must exclude match of query with document"s);
    }
}

// Тест проверяет, что возвращаемые при поиске документов результаты отсортированы
// в порядке убывания релевантности
void TestRelevanceDescendingSorting() {
    SearchServer server;
    server.AddDocument(0, "вкусный квас продается на площади"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "прохладный напиток на площади"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "кошара подкрался незаметно"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(3, "электричка и квас"s, DocumentStatus::ACTUAL, {1});

    // Проверяем, что количество найденных документов по прежнему 4
    // А также релевантность текущего документа >= релевантности следующего документа
    {
        const auto found_docs = server.FindTopDocuments("вкусный квас продается на площади"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 3u, "Number of documents must be the same after sorting by relevance"s);

        for (size_t i = 0; i < found_docs.size() - 1; ++i) {
            ASSERT_HINT(found_docs[i].relevance >= found_docs[i + 1].relevance, "Documents must be in descending order by relevance"s);
        }
    }
}

// Тест проверяет, что рейтинг добавленного документа равен среднему арифметическому
// оценок документа
void TestCorrectRatingComputing() {
    SearchServer server;
    server.AddDocument(0, "вкусный квас продается на площади"s, DocumentStatus::ACTUAL, {1, 2, 5});
    server.AddDocument(1, "прохладный напиток на площади"s, DocumentStatus::ACTUAL, {1, -1});
    server.AddDocument(2, "кошара подкрался незаметно"s, DocumentStatus::ACTUAL, {1, -2, 3, 0});
    server.AddDocument(3, "электричка и квас полный расколбас пивас"s, DocumentStatus::ACTUAL, {10});

    // Первый документ с id 0, его рейтинг = (int) (1 + 2 + 5) / 3 = 2 
    {
        const auto found_docs = server.FindTopDocuments("квас"s);
        ASSERT_EQUAL_HINT(found_docs[0].rating, 2, "Average rating of documents must equal arithmetic mean of all ratings"s);
    }

    // Первый документ с id 1, его рейтинг = (int) (-1 + 1) / 2 = 0
    {
        const auto found_docs = server.FindTopDocuments("прохладный"s);
        ASSERT_EQUAL(found_docs[0].rating, 0);
    }

    // Первый документ с id 3, его рейтинг = (int) (10) / 1 = 10
    {
        const auto found_docs = server.FindTopDocuments("электричка"s);
        ASSERT_EQUAL(found_docs[0].rating, 10);
    }
}

// Тест проверяет, что результаты поиска фильтруются с использованием пользовательского предиката
void TestSearchResultsFilteredWithPredicates() {
    SearchServer server;
    server.AddDocument(0, "вкусный квас продается на площади"s, DocumentStatus::ACTUAL, {1, 2, 5});
    server.AddDocument(1, "прохладный напиток на площади"s, DocumentStatus::ACTUAL, {1, -1});
    server.AddDocument(2, "кошара подкрался незаметно"s, DocumentStatus::ACTUAL, {1, -2, 3, 0});
    server.AddDocument(3, "электричка и квас полный расколбас пивас"s, DocumentStatus::ACTUAL, {10});

    // Находим документы с четными id
    {
        const auto found_docs = server.FindTopDocuments("вкусный прохладный квас"s, [] (int document_id, DocumentStatus document_status, int document_rating) {return document_id % 2 == 0;});
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 0);
    }

    // Находим документы с рейтингом >= 2
    {
        const auto found_docs = server.FindTopDocuments("квас на площади"s, [] (int document_id, DocumentStatus document_status, int document_rating) {return document_rating >= 2;});
        ASSERT_EQUAL(found_docs.size(), 2u);
        ASSERT_EQUAL(found_docs[0].id, 0);
    }
}

// Тест проверяет, что работает поиск документов с заданным статусом
void TestSearchDocumentsWithStatus() {
    SearchServer server;
    server.AddDocument(0, "вкусный квас продается на площади"s, DocumentStatus::ACTUAL, {1, 2, 5});
    server.AddDocument(1, "прохладный напиток на площади"s, DocumentStatus::BANNED, {1, -1});
    server.AddDocument(2, "кошара по кличке квас подкрался незаметно"s, DocumentStatus::ACTUAL, {1, -2, 3, 0});
    server.AddDocument(3, "электричка и квас полный расколбас пивас"s, DocumentStatus::IRRELEVANT, {10});
    server.AddDocument(4, "мужчина выпил квас и очутился на площади"s, DocumentStatus::REMOVED, {5, 5, 5});

    // Проверяем документы со статусом ACTUAL
    {
        const auto found_docs = server.FindTopDocuments("квас на площади"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 2u);
        ASSERT_EQUAL(found_docs[0].id, 0);
    }

    // Проверяем документы со статусом BANNED
    {
        const auto found_docs = server.FindTopDocuments("квас на площади"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 1);
    }

    // Проверяем документы со статусом IRRELEVANT
    {
        const auto found_docs = server.FindTopDocuments("квас на площади"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 3);
    }

    // Проверяем документы со статусом REMOVED
    {
        const auto found_docs = server.FindTopDocuments("квас на площади"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 4);
    }

}

// Тест проверяет, что релевантность найденных документов вычисляется корректно
void TestCorrectRelevanceComputing() {
SearchServer server;
    server.AddDocument(0, "вкусный квас продается на площади"s, DocumentStatus::ACTUAL, {1, 2, 5});
    server.AddDocument(1, "прохладный напиток на площади"s, DocumentStatus::ACTUAL, {1, -1});
    server.AddDocument(2, "кошара по кличке квас подкрался незаметно"s, DocumentStatus::ACTUAL, {1, -2, 3, 0});
    server.AddDocument(3, "электричка и квас полный расколбас пивас"s, DocumentStatus::ACTUAL, {10});

    // Сравниваем релевантности найденных документов с истинными
    {
        const auto found_docs = server.FindTopDocuments("квас на площади"s);
        const double top_doc_relevance1 = 0.346574;
        const double top_doc_relevance2 = 0.334795;
        const double top_doc_relevance3 = 0.047947;
        const double top_doc_relevance4 = 0.047947;
        const double EPSILON = 1e-6;

        ASSERT(abs(found_docs[0].relevance - top_doc_relevance1) < EPSILON);
        ASSERT(abs(found_docs[1].relevance - top_doc_relevance2) < EPSILON);
        ASSERT(abs(found_docs[2].relevance - top_doc_relevance3) < EPSILON);
        ASSERT(abs(found_docs[3].relevance - top_doc_relevance4) < EPSILON);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddedDocumentInQuery);
    RUN_TEST(TestStopWordsRemovedFromDocuments);
    RUN_TEST(TestMinusWordAreNotInSearchResult);
    RUN_TEST(TestDocumentsMatching);
    RUN_TEST(TestRelevanceDescendingSorting);
    RUN_TEST(TestCorrectRatingComputing);
    RUN_TEST(TestSearchResultsFilteredWithPredicates);
    RUN_TEST(TestSearchDocumentsWithStatus);
    RUN_TEST(TestCorrectRelevanceComputing);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}