// Including all neccessary libraries
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cmath>

using namespace std;

static const int MAX_RESULT_DOCUMENT_COUNT = 5;

// Reading whole string with spaces
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

// Reading integer value and whole string with spaces
int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

// Splitting string into words (separator is ' ') w/ stop words
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;

    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        }
        else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

// Document structure with its id and relevance
struct Document {
    int id;
    double relevance;
};


// ##############################################
// Main Search Server Class

class SearchServer {
public:
    // Setting stop words
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    // Adding words and ids to dictionary
    void AddDocument(int document_id, const string& document) {
        document_count_ += 1;
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();

        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
    }

    // Find top MAX_RESULT_DOCUMENT_COUNT docs by relevance
    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(), [] (const Document& doc1, const Document& doc2) {
            return doc1.relevance > doc2.relevance;
        });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

private:
    int document_count_ = 0;
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;

    // Struct with flags for -word and STOP_word
    struct QueryWord {
        string data;
        bool is_minus_word;
        bool is_stop_word;
    };

    // Struct for set of plus and minus words
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    // obvious :)
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    // Splitting string into words w/o stop words
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;

        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }

        return words;
    }

    // Setting QueryWord structure with values
    QueryWord ParseQueryWords(string text) const {
        bool is_minus_word = false;

        if (text[0] == '-') {
            is_minus_word = true;
            text = text.substr(1);
        }

        return {text, is_minus_word, IsStopWord(text)};
    }

    // Setting Query structure with values
    Query ParseQuery(const string& text) const {
        Query query;

        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWords(word);

            if (!query_word.is_stop_word) {
                if (query_word.is_minus_word) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }

        return query;
    }

    // Calculating term frequency
    double CalculateTPF(const string& word) const {
        return log(document_count_ * 1.0 / word_to_document_freqs_.at(word).size());
    }
    
    // Finding all relevant documents
    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;

        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            const double inverse_document_freq = CalculateTPF(word);

            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
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

        vector<Document> matched_document;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_document.push_back({document_id, relevance});
        }

        return matched_document;
    }
};

// Creating Search Server (constructor)
SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (auto [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = " << document_id << ", " << "relevance = " << relevance << " }" << endl;
    }

    return 0;
}