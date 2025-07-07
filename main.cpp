#include <tgbot/tgbot.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <random>
#include <algorithm>
#include <cctype>

struct QuizSession {
    std::vector<int> questions;  // индексы выбранных вопросов
    int currentIndex;
    int correctCount;
    std::vector<std::string> userAnswers;
};

int main() {
    using namespace std;
    // Загружаем вопросы из JSON-файла
    ifstream inFile("questions.json");
    if (!inFile.is_open()) {
        cerr << "Не удалось открыть файл questions.json" << endl;
        return 1;
    }
    nlohmann::json questionsJson;
    inFile >> questionsJson;
    inFile.close();

    // Инициализируем генератор случайных чисел
    std::mt19937_64 rng(static_cast<unsigned long long>(time(nullptr)));

    // Структуры данных для сессий пользователей и статистики
    unordered_map<long long, QuizSession> sessions;
    unordered_map<long long, int> statsTestsTaken;
    unordered_map<long long, int> statsBestScore;

    // Инициализируем бота (вставьте ваш токен)
    string token = "YOUR_TELEGRAM_BOT_TOKEN";
    TgBot::Bot bot(token);

    // Обработка команды /start
    bot.getEvents().onCommand("start", [&](TgBot::Message::Ptr message) {
        long long chatId = message->chat->id;
        string welcome = "Добро пожаловать! Это тестовый бот.\nНажмите /test, чтобы начать тест из 30 случайных вопросов.";
        bot.getApi().sendMessage(chatId, welcome);
    });

    // Обработка команды /test для запуска тестирования
    bot.getEvents().onCommand("test", [&](TgBot::Message::Ptr message) {
        long long chatId = message->chat->id;
        // Случайным образом выбираем 30 вопросов
        int totalQ = questionsJson.size();
        vector<int> indices(totalQ);
        for (int i = 0; i < totalQ; ++i) {
            indices[i] = i;
        }
        shuffle(indices.begin(), indices.end(), rng);
        if (indices.size() > 30) {
            indices.resize(30);
        }
        // Инициализируем сессию пользователя
        QuizSession session;
        session.questions = indices;
        session.currentIndex = 0;
        session.correctCount = 0;
        session.userAnswers.assign(session.questions.size(), "");
        sessions[chatId] = session;
        // Отправляем первый вопрос
        int qIndex = session.questions[0];
        auto qObj = questionsJson[qIndex];
        string qText = qObj["text"];
        if (qObj.contains("options")) {
            // Если вопрос с вариантами ответа (тип А)
            string msg = "Вопрос 1:\n" + qText + "\n";
            vector<string> opts = qObj["options"];
            char letter = 'A';
            for (const string& opt : opts) {
                msg += string(1, letter) + ") " + opt + "\n";
                letter++;
            }
            // Кнопки A, B, C, D
            TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
            vector<TgBot::InlineKeyboardButton::Ptr> row;
            for (char btn = 'A'; btn <= 'D'; ++btn) {
                TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
                button->text = string(1, btn);
                button->callbackData = string(1, btn);
                row.push_back(button);
            }
            keyboard->inlineKeyboard.push_back(row);
            bot.getApi().sendMessage(chatId, msg, false, 0, keyboard);
        } else {
            // Вопрос с открытым ответом (тип Б)
            string msg = "Вопрос 1:\n" + qText + "\n(Введите свой ответ сообщением.)";
            bot.getApi().sendMessage(chatId, msg);
        }
    });

    // Обработка нажатий кнопок (варианты ответа и финальное меню)
    bot.getEvents().onCallbackQuery([&](TgBot::CallbackQuery::Ptr query) {
        long long chatId = query->message->chat->id;
        string data = query->data;
        if (sessions.find(chatId) == sessions.end()) {
            bot.getApi().answerCallbackQuery(query->id);
            return;
        }
        // Обработка кнопок финального меню
        if (data == "retry") {
            bot.getApi().answerCallbackQuery(query->id);
            // Перезапустить тест заново
            if (statsTestsTaken.find(chatId) == statsTestsTaken.end()) {
                statsTestsTaken[chatId] = 0;
                statsBestScore[chatId] = 0;
            }
            // Генерируем новый набор вопросов
            int totalQ = questionsJson.size();
            vector<int> indices(totalQ);
            for (int i = 0; i < totalQ; ++i) indices[i] = i;
            shuffle(indices.begin(), indices.end(), rng);
            if (indices.size() > 30) {
                indices.resize(30);
            }
            QuizSession session;
            session.questions = indices;
            session.currentIndex = 0;
            session.correctCount = 0;
            session.userAnswers.assign(session.questions.size(), "");
            sessions[chatId] = session;
            // Отправляем первый вопрос нового теста
            int qIndex = session.questions[0];
            auto qObj = questionsJson[qIndex];
            string qText = qObj["text"];
            if (qObj.contains("options")) {
                string msg = "Вопрос 1:\n" + qText + "\n";
                vector<string> opts = qObj["options"];
                char letter = 'A';
                for (const string& opt : opts) {
                    msg += string(1, letter) + ") " + opt + "\n";
                    letter++;
                }
                TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
                vector<TgBot::InlineKeyboardButton::Ptr> row;
                for (char btn = 'A'; btn <= 'D'; ++btn) {
                    TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
                    button->text = string(1, btn);
                    button->callbackData = string(1, btn);
                    row.push_back(button);
                }
                keyboard->inlineKeyboard.push_back(row);
                bot.getApi().sendMessage(chatId, msg, false, 0, keyboard);
            } else {
                string msg = "Вопрос 1:\n" + qText + "\n(Введите свой ответ сообщением.)";
                bot.getApi().sendMessage(chatId, msg);
            }
            return;
        }
        if (data == "theory") {
            bot.getApi().answerCallbackQuery(query->id);
            string theoryMsg = "Рекомендуем изучить теоретические материалы курса для улучшения результатов.";
            bot.getApi().sendMessage(chatId, theoryMsg);
            return;
        }
        if (data == "stats") {
            bot.getApi().answerCallbackQuery(query->id);
            int taken = statsTestsTaken[chatId];
            int best = statsBestScore[chatId];
            string statsMsg = "Вы прошли тестов: " + to_string(taken);
            if (taken > 0) {
                statsMsg += "\nЛучший результат: " + to_string(best) + " из 30";
            }
            bot.getApi().sendMessage(chatId, statsMsg);
            return;
        }
        // Если нажата кнопка с вариантом ответа (A, B, C, D)
        bot.getApi().answerCallbackQuery(query->id);
        QuizSession& session = sessions[chatId];
        int cur = session.currentIndex;
        // Сохраняем ответ пользователя
        session.userAnswers[cur] = data;
        // Проверяем правильность
        int qIndex = session.questions[cur];
        char correctChar = 0;
        if (questionsJson[qIndex].contains("correct")) {
            string corr = questionsJson[qIndex]["correct"];
            if (!corr.empty()) {
                correctChar = corr[0];
            }
        }
        if (correctChar != 0 && data.size() == 1 && toupper(data[0]) == toupper(correctChar)) {
            session.correctCount += 1;
        }
        session.currentIndex += 1;
        // Отправляем следующий вопрос или завершаем тест
        if (session.currentIndex < (int)session.questions.size()) {
            int nextIndex = session.currentIndex;
            int qIdx = session.questions[nextIndex];
            auto qObj = questionsJson[qIdx];
            string qText2 = qObj["text"];
            int qNumber = nextIndex + 1;
            if (qObj.contains("options")) {
                string msg = "Вопрос " + to_string(qNumber) + ":\n" + qText2 + "\n";
                vector<string> opts = qObj["options"];
                char letter = 'A';
                for (const string& opt : opts) {
                    msg += string(1, letter) + ") " + opt + "\n";
                    letter++;
                }
                TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
                vector<TgBot::InlineKeyboardButton::Ptr> row;
                for (char btn = 'A'; btn <= 'D'; ++btn) {
                    TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
                    button->text = string(1, btn);
                    button->callbackData = string(1, btn);
                    row.push_back(button);
                }
                keyboard->inlineKeyboard.push_back(row);
                bot.getApi().sendMessage(chatId, msg, false, 0, keyboard);
            } else {
                string msg = "Вопрос " + to_string(qNumber) + ":\n" + qText2 + "\n(Введите свой ответ сообщением.)";
                bot.getApi().sendMessage(chatId, msg);
            }
        } else {
            // Завершение теста
            int total = session.questions.size();
            int correctCount = session.correctCount;
            // Обновляем статистику
            statsTestsTaken[chatId] += 1;
            if (statsBestScore.find(chatId) == statsBestScore.end() || correctCount > statsBestScore[chatId]) {
                statsBestScore[chatId] = correctCount;
            }
            // Формируем отчет
            string resultMsg = "Тест завершен!\nВаш результат: " + to_string(correctCount) + " из " + to_string(total) + " правильных ответов.\n\n";
            resultMsg += "=== Подробный отчет ===\n";
            for (int i = 0; i < total; ++i) {
                int qIdx = session.questions[i];
                string qT = questionsJson[qIdx]["text"];
                resultMsg += to_string(i+1) + ". " + qT + "\n";
                string userAns = session.userAnswers[i];
                string correctAns;
                if (questionsJson[qIdx].contains("options")) {
                    string corrLetter = questionsJson[qIdx]["correct"];
                    correctAns = corrLetter;
                } else {
                    string corrText = questionsJson[qIdx]["answer"];
                    correctAns = corrText;
                }
                resultMsg += "Ваш ответ: " + (userAns.empty() ? "(нет ответа)" : userAns) + "\n";
                resultMsg += "Правильный ответ: " + correctAns + "\n\n";
            }
            bot.getApi().sendMessage(chatId, resultMsg);
            // Кнопки финальных действий
            TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
            vector<TgBot::InlineKeyboardButton::Ptr> row;
            TgBot::InlineKeyboardButton::Ptr btn1(new TgBot::InlineKeyboardButton);
            btn1->text = "Пройти ещё раз";
            btn1->callbackData = "retry";
            TgBot::InlineKeyboardButton::Ptr btn2(new TgBot::InlineKeyboardButton);
            btn2->text = "Изучить теорию";
            btn2->callbackData = "theory";
            TgBot::InlineKeyboardButton::Ptr btn3(new TgBot::InlineKeyboardButton);
            btn3->text = "Моя статистика";
            btn3->callbackData = "stats";
            row.push_back(btn1);
            row.push_back(btn2);
            row.push_back(btn3);
            keyboard->inlineKeyboard.push_back(row);
            bot.getApi().sendMessage(chatId, "Выберите дальнейшее действие:", false, 0, keyboard);
            // Чистим сессию
            sessions.erase(chatId);
        }
    });

    // Обработка текстовых сообщений (для открытых вопросов)
    bot.getEvents().onAnyMessage([&](TgBot::Message::Ptr message) {
        if (StringTools::startsWith(message->text, "/")) {
            // Игнорируем команды
            return;
        }
        long long chatId = message->chat->id;
        if (sessions.find(chatId) == sessions.end()) {
            return;
        }
        QuizSession& session = sessions[chatId];
        int cur = session.currentIndex;
        // Обрабатываем только если текущий вопрос ожидает текстовый ответ
        int qIndex = session.questions[cur];
        if (questionsJson[qIndex].contains("options")) {
            // Если это вопрос с вариантами (пользователь вместо нажатия кнопки отправил текст)
            return;
        }
        // Сохраняем ответ пользователя
        string userAnswer = message->text;
        session.userAnswers[cur] = userAnswer;
        // Проверяем правильность ответа (без учета регистра и лишних пробелов)
        string correctAns = questionsJson[qIndex]["answer"];
        string ua = userAnswer;
        string ca = correctAns;
        auto toUpperStr = [&](string &s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
        };
        toUpperStr(ua);
        toUpperStr(ca);
        // Тримминг пробелов
        auto trim = [&](string &s) {
            while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
            while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
        };
        trim(ua);
        trim(ca);
        if (!ua.empty() && ua == ca) {
            session.correctCount += 1;
        }
        session.currentIndex += 1;
        // Отправляем следующий вопрос или подводим итог
        if (session.currentIndex < (int)session.questions.size()) {
            int nextIndex = session.currentIndex;
            int qIdx = session.questions[nextIndex];
            auto qObj = questionsJson[qIdx];
            string qText2 = qObj["text"];
            int qNumber = nextIndex + 1;
            if (qObj.contains("options")) {
                string msg = "Вопрос " + to_string(qNumber) + ":\n" + qText2 + "\n";
                vector<string> opts = qObj["options"];
                char letter = 'A';
                for (const string& opt : opts) {
                    msg += string(1, letter) + ") " + opt + "\n";
                    letter++;
                }
                TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
                vector<TgBot::InlineKeyboardButton::Ptr> row;
                for (char btn = 'A'; btn <= 'D'; ++btn) {
                    TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
                    button->text = string(1, btn);
                    button->callbackData = string(1, btn);
                    row.push_back(button);
                }
                keyboard->inlineKeyboard.push_back(row);
                bot.getApi().sendMessage(chatId, msg, false, 0, keyboard);
            } else {
                string msg = "Вопрос " + to_string(qNumber) + ":\n" + qText2 + "\n(Введите свой ответ сообщением.)";
                bot.getApi().sendMessage(chatId, msg);
            }
        } else {
            // Завершение теста (аналогично обработке в onCallbackQuery)
            int total = session.questions.size();
            int correctCount = session.correctCount;
            statsTestsTaken[chatId] += 1;
            if (statsBestScore.find(chatId) == statsBestScore.end() || correctCount > statsBestScore[chatId]) {
                statsBestScore[chatId] = correctCount;
            }
            string resultMsg = "Тест завершен!\nВаш результат: " + to_string(correctCount) + " из " + to_string(total) + " правильных ответов.\n\n";
            resultMsg += "=== Подробный отчет ===\n";
            for (int i = 0; i < total; ++i) {
                int qIdx = session.questions[i];
                string qT = questionsJson[qIdx]["text"];
                resultMsg += to_string(i+1) + ". " + qT + "\n";
                string userAns = session.userAnswers[i];
                string correctAns2;
                if (questionsJson[qIdx].contains("options")) {
                    string corrLetter = questionsJson[qIdx]["correct"];
                    correctAns2 = corrLetter;
                } else {
                    correctAns2 = (string)questionsJson[qIdx]["answer"];
                }
                resultMsg += "Ваш ответ: " + (userAns.empty() ? "(нет ответа)" : userAns) + "\n";
                resultMsg += "Правильный ответ: " + correctAns2 + "\n\n";
            }
            bot.getApi().sendMessage(chatId, resultMsg);
            TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
            vector<TgBot::InlineKeyboardButton::Ptr> row;
            TgBot::InlineKeyboardButton::Ptr btn1(new TgBot::InlineKeyboardButton);
            btn1->text = "Пройти ещё раз";
            btn1->callbackData = "retry";
            TgBot::InlineKeyboardButton::Ptr btn2(new TgBot::InlineKeyboardButton);
            btn2->text = "Изучить теорию";
            btn2->callbackData = "theory";
            TgBot::InlineKeyboardButton::Ptr btn3(new TgBot::InlineKeyboardButton);
            btn3->text = "Моя статистика";
            btn3->callbackData = "stats";
            row.push_back(btn1);
            row.push_back(btn2);
            row.push_back(btn3);
            keyboard->inlineKeyboard.push_back(row);
            bot.getApi().sendMessage(chatId, "Выберите дальнейшее действие:", false, 0, keyboard);
            sessions.erase(chatId);
        }
    });

    try {
        TgBot::TgLongPoll longPoll(bot);
        cout << "Bot started. Press Ctrl+C to stop." << endl;
        while (true) {
            longPoll.start();
        }
    } catch (exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}
