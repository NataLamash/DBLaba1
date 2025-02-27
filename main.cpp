#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <string>
#include <cstdio> // для remove та rename

struct User {
    int userID;
    char name[30];
    char email[50];
    int firstReviewIndex; // індекс першого відгуку у слейв файлі
    int reviewCount;
    bool isDeleted;
};

struct Review {
    int reviewID;
    int userID;
    char content[100];
    int nextReviewIndex; // індекс наступного відгуку у ланцюжку
    bool isDeleted;
};

struct UserIndex {
    int userID;
    int recordIndex;
};

std::vector<int> userTrash;   // Сміттєва зона для видалених записів користувачів
std::vector<int> reviewTrash; // Сміттєва зона для видалених записів відгуків

// Функція перевірки існування файлу
bool fileExists(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    return file.good();
}

// Функції створення початкових файлів (будуть викликані лише при першому запуску)
void createMaster() {
    std::ofstream masterFile("users.fl", std::ios::binary | std::ios::trunc);
    std::ofstream indexFile("users.ind", std::ios::binary | std::ios::trunc);

    User users[] = {
        {1, "John", "john@example.com", 0, 2, false},
        {2, "Alice", "alice@example.com", 2, 1, false},
        {3, "Bob", "bob@example.com", -1, 0, false}
    };

    for (int i = 0; i < 3; ++i) {
        masterFile.write(reinterpret_cast<char*>(&users[i]), sizeof(User));
        UserIndex index = {users[i].userID, i};
        indexFile.write(reinterpret_cast<char*>(&index), sizeof(UserIndex));
    }

    masterFile.close();
    indexFile.close();
}

void createSlave() {
    std::ofstream slaveFile("reviews.fl", std::ios::binary | std::ios::trunc);

    Review reviews[] = {
        {1, 1, "Great service!", 1, false},
        {2, 1, "Quick response.", -1, false},
        {3, 2, "Excellent support!", -1, false}
    };

    for (int i = 0; i < 3; ++i) {
        slaveFile.write(reinterpret_cast<char*>(&reviews[i]), sizeof(Review));
    }

    slaveFile.close();
}

// Функція для доступу до користувача через майстер-файл з використанням бінарного пошуку по відсортованій індексній таблиці
void getMaster(int userID) {
    std::ifstream indexFile("users.ind", std::ios::binary);
    std::vector<UserIndex> indexTable;
    UserIndex index;
    while (indexFile.read(reinterpret_cast<char*>(&index), sizeof(UserIndex))) {
        indexTable.push_back(index);
    }
    indexFile.close();

    // Сортування за userID
    std::sort(indexTable.begin(), indexTable.end(), [](const UserIndex& a, const UserIndex& b) {
        return a.userID < b.userID;
    });

    // Бінарний пошук
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), userID, [](const UserIndex& idx, int value) {
        return idx.userID < value;
    });

    if (it != indexTable.end() && it->userID == userID) {
        int recordIndex = it->recordIndex;
        std::ifstream masterFile("users.fl", std::ios::binary);
        masterFile.seekg(recordIndex * sizeof(User));
        User user;
        masterFile.read(reinterpret_cast<char*>(&user), sizeof(User));
        masterFile.close();
        if (!user.isDeleted) {
            std::cout << "UserID: " << user.userID << "\nName: " << user.name
                      << "\nEmail: " << user.email << "\nReviewCount: " << user.reviewCount << std::endl;
        } else {
            std::cout << "Record is deleted." << std::endl;
        }
    } else {
        std::cout << "User not found." << std::endl;
    }
}

void getSlave(int userID) {
    std::ifstream masterFile("users.fl", std::ios::binary);
    User user;
    bool found = false;
    while (masterFile.read(reinterpret_cast<char*>(&user), sizeof(User))) {
        if (user.userID == userID && !user.isDeleted) {
            found = true;
            int reviewIndex = user.firstReviewIndex;
            std::ifstream reviewFile("reviews.fl", std::ios::binary);
            while (reviewIndex != -1) {
                reviewFile.seekg(reviewIndex * sizeof(Review));
                Review review;
                reviewFile.read(reinterpret_cast<char*>(&review), sizeof(Review));
                if (!review.isDeleted) {
                    std::cout << "ReviewID: " << review.reviewID << "\nContent: " << review.content << std::endl;
                }
                reviewIndex = review.nextReviewIndex;
            }
            reviewFile.close();
            break;
        }
    }
    if (!found) {
        std::cout << "User not found or deleted." << std::endl;
    }
    masterFile.close();
}

// Прототип функції delSlave
void delSlave(int reviewIndex);

// Оновлена функція видалення користувача (майстер) з викликом оновленого видалення відгуків
void delMaster(int userID) {
    std::fstream masterFile("users.fl", std::ios::binary | std::ios::in | std::ios::out);
    std::ifstream indexFile("users.ind", std::ios::binary);
    std::vector<UserIndex> indexTable;
    UserIndex index;
    int recordIndex = -1;
    while (indexFile.read(reinterpret_cast<char*>(&index), sizeof(UserIndex))) {
        if (index.userID == userID) {
            recordIndex = index.recordIndex;
        } else {
            indexTable.push_back(index);
        }
    }
    indexFile.close();

    if (recordIndex == -1) {
        std::cout << "User not found." << std::endl;
        return;
    }

    User user;
    masterFile.seekg(recordIndex * sizeof(User));
    masterFile.read(reinterpret_cast<char*>(&user), sizeof(User));

    // Видалення всіх пов’язаних відгуків із оновленням ланцюжка
    int reviewIndex = user.firstReviewIndex;
    while (reviewIndex != -1) {
        std::ifstream reviewFile("reviews.fl", std::ios::binary);
        reviewFile.seekg(reviewIndex * sizeof(Review));
        Review review;
        reviewFile.read(reinterpret_cast<char*>(&review), sizeof(Review));
        int nextIndex = review.nextReviewIndex;
        reviewFile.close();

        delSlave(reviewIndex);

        reviewIndex = nextIndex;
    }

    user.isDeleted = true;
    masterFile.seekp(recordIndex * sizeof(User));
    masterFile.write(reinterpret_cast<char*>(&user), sizeof(User));
    masterFile.close();

    userTrash.push_back(recordIndex);

    std::ofstream indexFileOut("users.ind", std::ios::binary | std::ios::trunc);
    
    // Записуємо лише записи активних користувачів
    for (const auto& idx : indexTable) {
        indexFileOut.write(reinterpret_cast<const char*>(&idx), sizeof(UserIndex));
    }
    indexFileOut.close();
}

// Оновлена функція видалення відгуку (слейв) з оновленням ланцюжка зв’язування
void delSlave(int reviewIndex) {
    // Відкриваємо файл відгуків для позначення видалення
    std::fstream reviewFile("reviews.fl", std::ios::binary | std::ios::in | std::ios::out);
    Review reviewToDelete;
    reviewFile.seekg(reviewIndex * sizeof(Review));
    reviewFile.read(reinterpret_cast<char*>(&reviewToDelete), sizeof(Review));
    if (reviewToDelete.isDeleted) {
        reviewFile.close();
        return;
    }
    int userID = reviewToDelete.userID;
    int nextIndex = reviewToDelete.nextReviewIndex;
    reviewToDelete.isDeleted = true;
    reviewFile.seekp(reviewIndex * sizeof(Review));
    reviewFile.write(reinterpret_cast<char*>(&reviewToDelete), sizeof(Review));
    reviewFile.close();

    // Оновлюємо зв'язок: спочатку перевіряємо, чи є видаляємий відгук першим у ланцюжку користувача
    std::fstream masterFile("users.fl", std::ios::binary | std::ios::in | std::ios::out);
    User user;
    int userPos = -1;
    int pos = 0;
    bool found = false;
    masterFile.seekg(0, std::ios::beg);
    while (masterFile.read(reinterpret_cast<char*>(&user), sizeof(User))) {
        if (user.userID == userID && !user.isDeleted) {
            userPos = pos;
            found = true;
            break;
        }
        pos++;
    }
    if (found) {
        if (user.firstReviewIndex == reviewIndex) {
            user.firstReviewIndex = nextIndex;
            if(user.reviewCount > 0) user.reviewCount--;
            masterFile.seekp(userPos * sizeof(User));
            masterFile.write(reinterpret_cast<char*>(&user), sizeof(User));
            masterFile.close();
            reviewTrash.push_back(reviewIndex);
            return;
        }
    }
    masterFile.close();

    // Якщо відгук не є першим, шукаємо попередній елемент у ланцюжку
    std::fstream slaveFile("reviews.fl", std::ios::binary | std::ios::in | std::ios::out);
    // Отримуємо актуальний firstReviewIndex користувача з файлу
    std::ifstream masterFileIn("users.fl", std::ios::binary);
    User user2;
    int userFirstReviewIndex = -1;
    while (masterFileIn.read(reinterpret_cast<char*>(&user2), sizeof(User))) {
        if (user2.userID == userID && !user2.isDeleted) {
            userFirstReviewIndex = user2.firstReviewIndex;
            break;
        }
    }
    masterFileIn.close();
    if (userFirstReviewIndex == -1) {
        slaveFile.close();
        return;
    }
    int currentIndex = userFirstReviewIndex;
    while (currentIndex != -1) {
        Review currentReview;
        slaveFile.seekg(currentIndex * sizeof(Review));
        slaveFile.read(reinterpret_cast<char*>(&currentReview), sizeof(Review));
        if (currentReview.nextReviewIndex == reviewIndex) {
            currentReview.nextReviewIndex = nextIndex;
            slaveFile.seekp(currentIndex * sizeof(Review));
            slaveFile.write(reinterpret_cast<char*>(&currentReview), sizeof(Review));
            break;
        }
        currentIndex = currentReview.nextReviewIndex;
    }
    slaveFile.close();
    reviewTrash.push_back(reviewIndex);
}

void insertMaster() {
    std::fstream masterFile("users.fl", std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
    std::ofstream indexFile("users.ind", std::ios::binary | std::ios::app);

    User user;
    std::cout << "Enter userID, name, email: ";
    std::cin >> user.userID >> user.name >> user.email;
    user.firstReviewIndex = -1;
    user.reviewCount = 0;
    user.isDeleted = false;

    int recordIndex;
    if (!userTrash.empty()) {
        recordIndex = userTrash.back();
        userTrash.pop_back();
        masterFile.seekp(recordIndex * sizeof(User));
    } else {
        masterFile.seekp(0, std::ios::end);
        recordIndex = masterFile.tellp() / sizeof(User);
    }

    masterFile.write(reinterpret_cast<char*>(&user), sizeof(User));
    masterFile.close();

    UserIndex index = {user.userID, recordIndex};
    indexFile.write(reinterpret_cast<char*>(&index), sizeof(UserIndex));
    indexFile.close();
}

void insertSlave() {
    std::fstream slaveFile("reviews.fl", std::ios::binary | std::ios::in | std::ios::out | std::ios::app);

    Review review;
    std::cout << "Enter reviewID, userID, content: ";
    std::cin >> review.reviewID >> review.userID;
    std::cin.ignore();
    std::cin.getline(review.content, 100);
    review.isDeleted = false;

    // Знаходимо користувача для вставки відгуку
    std::fstream masterFile("users.fl", std::ios::binary | std::ios::in | std::ios::out);
    int userIndex = -1;
    User user;
    while (masterFile.read(reinterpret_cast<char*>(&user), sizeof(User))) {
        if (user.userID == review.userID && !user.isDeleted) {
            userIndex = static_cast<int>(masterFile.tellg() / sizeof(User)) - 1;
            break;
        }
    }

    if (userIndex == -1) {
        std::cout << "User not found." << std::endl;
        masterFile.close();
        return;
    }

    int reviewIndex;
    if (!reviewTrash.empty()) {
        reviewIndex = reviewTrash.back();
        reviewTrash.pop_back();
        slaveFile.seekp(reviewIndex * sizeof(Review));
    } else {
        slaveFile.seekp(0, std::ios::end);
        reviewIndex = slaveFile.tellp() / sizeof(Review);
    }

    // Новий відгук вставляється на початок ланцюжка
    review.nextReviewIndex = user.firstReviewIndex;
    user.firstReviewIndex = reviewIndex;
    user.reviewCount++;

    slaveFile.write(reinterpret_cast<char*>(&review), sizeof(Review));
    slaveFile.close();

    masterFile.seekp(userIndex * sizeof(User));
    masterFile.write(reinterpret_cast<char*>(&user), sizeof(User));
    masterFile.close();
}

void updateMaster() {
    int userID;
    std::cout << "Enter userID to update: ";
    std::cin >> userID;

    std::ifstream indexFile("users.ind", std::ios::binary);
    UserIndex index;
    int recordIndex = -1;
    while (indexFile.read(reinterpret_cast<char*>(&index), sizeof(UserIndex))) {
        if (index.userID == userID) {
            recordIndex = index.recordIndex;
            break;
        }
    }
    indexFile.close();

    if (recordIndex == -1) {
        std::cout << "User not found." << std::endl;
        return;
    }

    std::fstream masterFile("users.fl", std::ios::binary | std::ios::in | std::ios::out);
    masterFile.seekg(recordIndex * sizeof(User));
    User user;
    masterFile.read(reinterpret_cast<char*>(&user), sizeof(User));

    if (user.isDeleted) {
        std::cout << "User is deleted." << std::endl;
        masterFile.close();
        return;
    }

    std::cout << "Enter field to update (name/email): ";
    std::string field;
    std::cin >> field;
    std::cin.ignore();

    if (field == "name") {
        std::cout << "Enter new name: ";
        std::cin.getline(user.name, 30);
    } else if (field == "email") {
        std::cout << "Enter new email: ";
        std::cin.getline(user.email, 50);
    } else {
        std::cout << "Invalid field." << std::endl;
        masterFile.close();
        return;
    }

    masterFile.seekp(recordIndex * sizeof(User));
    masterFile.write(reinterpret_cast<char*>(&user), sizeof(User));
    masterFile.close();

    std::cout << "User updated successfully." << std::endl;
}

void updateSlave() {
    int reviewIndex;
    std::cout << "Enter review index to update: ";
    std::cin >> reviewIndex;

    std::fstream reviewFile("reviews.fl", std::ios::binary | std::ios::in | std::ios::out);
    reviewFile.seekg(reviewIndex * sizeof(Review));
    Review review;
    reviewFile.read(reinterpret_cast<char*>(&review), sizeof(Review));

    if (review.isDeleted) {
        std::cout << "Review is deleted." << std::endl;
        reviewFile.close();
        return;
    }

    std::cout << "Enter field to update (content): ";
    std::string field;
    std::cin >> field;
    std::cin.ignore();

    if (field == "content") {
        std::cout << "Enter new content: ";
        std::cin.getline(review.content, 100);
    } else {
        std::cout << "Invalid field." << std::endl;
        reviewFile.close();
        return;
    }

    reviewFile.seekp(reviewIndex * sizeof(Review));
    reviewFile.write(reinterpret_cast<char*>(&review), sizeof(Review));
    reviewFile.close();

    std::cout << "Review updated successfully." << std::endl;
}

void calcMaster() {
    std::ifstream masterFile("users.fl", std::ios::binary);
    if (!masterFile) {
        std::cout << "Failed to open users.fl" << std::endl;
        return;
    }

    int totalCount = 0;
    int activeCount = 0;
    User user;
    while (masterFile.read(reinterpret_cast<char*>(&user), sizeof(User))) {
        totalCount++;
        if (!user.isDeleted)
            activeCount++;
    }
    masterFile.close();
    std::cout << "Total users: " << totalCount << ", Active users: " << activeCount << std::endl;
}

void calcSlave() {
    std::ifstream masterFile("users.fl", std::ios::binary);
    std::ifstream reviewFile("reviews.fl", std::ios::binary);
    if (!masterFile || !reviewFile) {
        std::cout << "Failed to open files" << std::endl;
        return;
    }

    int totalCount = 0;
    int activeCount = 0;
    User user;
    while (masterFile.read(reinterpret_cast<char*>(&user), sizeof(User))) {
        if (user.isDeleted)
            continue;

        int userReviewCount = 0;
        int reviewIndex = user.firstReviewIndex;
        while (reviewIndex != -1) {
            reviewFile.seekg(reviewIndex * sizeof(Review));
            Review review;
            reviewFile.read(reinterpret_cast<char*>(&review), sizeof(Review));
            totalCount++;
            if (!review.isDeleted) {
                activeCount++;
                userReviewCount++;
            }
            reviewIndex = review.nextReviewIndex;
        }
        std::cout << "UserID: " << user.userID << " has " << userReviewCount << " active reviews." << std::endl;
    }
    masterFile.close();
    reviewFile.close();
    std::cout << "Total reviews: " << totalCount << ", Active reviews: " << activeCount << std::endl;
}

void utMaster() {
    std::ifstream masterFile("users.fl", std::ios::binary);
    if (!masterFile) {
        std::cout << "Failed to open users.fl" << std::endl;
        return;
    }
    User user;
    int index = 0;
    while (masterFile.read(reinterpret_cast<char*>(&user), sizeof(User))) {
        std::cout << "Index: " << index << " | UserID: " << user.userID << " | Name: " << user.name
                  << " | Email: " << user.email << " | FirstReviewIndex: " << user.firstReviewIndex
                  << " | ReviewCount: " << user.reviewCount << " | isDeleted: " << user.isDeleted << std::endl;
        ++index;
    }
    masterFile.close();
}

void utSlave() {
    std::ifstream slaveFile("reviews.fl", std::ios::binary);
    if (!slaveFile) {
        std::cout << "Failed to open reviews.fl" << std::endl;
        return;
    }
    Review review;
    int index = 0;
    while (slaveFile.read(reinterpret_cast<char*>(&review), sizeof(Review))) {
        std::cout << "Index: " << index << " | ReviewID: " << review.reviewID << " | UserID: " << review.userID
                  << " | Content: " << review.content << " | NextReviewIndex: " << review.nextReviewIndex
                  << " | isDeleted: " << review.isDeleted << std::endl;
        ++index;
    }
    slaveFile.close();
}

// Функції для фізичної реорганізації файлів

// Функція реорганізації майстер-файлу із перенумерацією UserID (починаючи з 1)
// Повертає мапу, що зіставляє старі UserID з новими
std::unordered_map<int,int> reorganizeMaster() {
    std::ifstream masterIn("users.fl", std::ios::binary);
    std::vector<User> newUsers;
    std::vector<UserIndex> newIndex;
    User user;
    std::unordered_map<int,int> userIdMapping; // старий -> новий UserID

    while (masterIn.read(reinterpret_cast<char*>(&user), sizeof(User))) {
        if (!user.isDeleted) {
            newUsers.push_back(user);
        }
    }
    masterIn.close();

    // Присвоюємо нові UserID, починаючи з 1, та формуємо індексну таблицю
    for (size_t i = 0; i < newUsers.size(); i++) {
        int newId = static_cast<int>(i + 1);
        userIdMapping[newUsers[i].userID] = newId;  // зберігаємо відповідність: старий -> новий
        newUsers[i].userID = newId;
        newIndex.push_back({newId, static_cast<int>(i)});
    }

    std::ofstream masterOut("users_new.fl", std::ios::binary | std::ios::trunc);
    for (const auto &u : newUsers) {
        masterOut.write(reinterpret_cast<const char*>(&u), sizeof(User));
    }
    masterOut.close();
    remove("users.fl");
    rename("users_new.fl", "users.fl");

    // Оновлюємо індексну таблицю (вже відсортована за новим UserID)
    std::sort(newIndex.begin(), newIndex.end(), [](const UserIndex &a, const UserIndex &b) {
        return a.userID < b.userID;
    });
    std::ofstream indexOut("users.ind", std::ios::binary | std::ios::trunc);
    for (const auto &idx : newIndex) {
        indexOut.write(reinterpret_cast<const char*>(&idx), sizeof(UserIndex));
    }
    indexOut.close();

    return userIdMapping;
}

// Функція реорганізації слейв файлу із перенумерацією ReviewID (починаючи з 1)
// А також оновлює поле userID відгуків згідно з новими значеннями
void reorganizeSlave(const std::unordered_map<int,int>& userIdMapping) {
    std::ifstream slaveIn("reviews.fl", std::ios::binary);
    std::vector<Review> newReviews;
    // Мапа: старий індекс -> новий індекс
    std::unordered_map<int, int> reviewMapping;
    int oldIndex = 0;
    Review review;
    while (slaveIn.read(reinterpret_cast<char*>(&review), sizeof(Review))) {
        if (!review.isDeleted) {
            int newReviewId = static_cast<int>(newReviews.size() + 1);
            review.reviewID = newReviewId;
            // Оновлюємо поле userID згідно з новим UserID
            auto it = userIdMapping.find(review.userID);
            if (it != userIdMapping.end())
                review.userID = it->second;
            else
                review.userID = 0;
            int newIndex = newReviews.size();
            newReviews.push_back(review);
            reviewMapping[oldIndex] = newIndex;
        }
        oldIndex++;
    }
    slaveIn.close();

    // Оновлюємо посилання (nextReviewIndex) у нових записах
    for (auto &r : newReviews) {
        if (r.nextReviewIndex != -1) {
            if (reviewMapping.find(r.nextReviewIndex) != reviewMapping.end())
                r.nextReviewIndex = reviewMapping[r.nextReviewIndex];
            else
                r.nextReviewIndex = -1;
        }
    }

    std::ofstream slaveOut("reviews_new.fl", std::ios::binary | std::ios::trunc);
    for (const auto &r : newReviews) {
        slaveOut.write(reinterpret_cast<const char*>(&r), sizeof(Review));
    }
    slaveOut.close();
    remove("reviews.fl");
    rename("reviews_new.fl", "reviews.fl");

    // Оновлюємо посилання в майстер-файлі (firstReviewIndex)
    std::fstream masterFile("users.fl", std::ios::binary | std::ios::in | std::ios::out);
    int userIndex = 0;
    while (masterFile.peek() != EOF) {
        User user;
        masterFile.read(reinterpret_cast<char*>(&user), sizeof(User));
        if (user.firstReviewIndex != -1) {
            if (reviewMapping.find(user.firstReviewIndex) != reviewMapping.end())
                user.firstReviewIndex = reviewMapping[user.firstReviewIndex];
            else
                user.firstReviewIndex = -1;
        }
        masterFile.seekp(userIndex * sizeof(User));
        masterFile.write(reinterpret_cast<char*>(&user), sizeof(User));
        userIndex++;
    }
    masterFile.close();
}

// Загальна функція для реорганізації обох файлів
void reorganizeFiles() {
    // Спочатку перебудовуємо майстер-файл і отримуємо відповідність старих і нових UserID
    std::unordered_map<int,int> userIdMapping = reorganizeMaster();
    // Потім перебудовуємо слейв файл із оновленням ReviewID та userID
    reorganizeSlave(userIdMapping);
    userTrash.clear();
    reviewTrash.clear();
    std::cout << "Files reorganized successfully." << std::endl;
}

int main() {
    // Ініціалізація файлів виконується лише якщо вони відсутні
    if (!fileExists("users.fl") || !fileExists("users.ind"))
        createMaster();
    if (!fileExists("reviews.fl"))
        createSlave();

    std::string command;
    int id;

    while (true) {
        std::cout << "\nEnter command (get-m, get-s, del-m, del-s, insert-m, insert-s, update-m, update-s, calc-m, calc-s, ut-m, ut-s, reorg, exit): ";
        std::cin >> command;
        if (command == "get-m") {
            std::cin >> id;
            getMaster(id);
        } else if (command == "get-s") {
            std::cin >> id;
            getSlave(id);
        } else if (command == "del-m") {
            std::cin >> id;
            delMaster(id);
        } else if (command == "del-s") {
            std::cin >> id;
            delSlave(id);
        } else if (command == "insert-m") {
            insertMaster();
        } else if (command == "insert-s") {
            insertSlave();
        } else if (command == "update-m") {
            updateMaster();
        } else if (command == "update-s") {
            updateSlave();
        } else if (command == "calc-m") {
            calcMaster();
        } else if (command == "calc-s") {
            calcSlave();
        } else if (command == "ut-m") {
            utMaster();
        } else if (command == "ut-s") {
            utSlave();
        } else if (command == "reorg") {
            reorganizeFiles();
        } else if (command == "exit") {
            break;
        } else {
            std::cout << "Unknown command!" << std::endl;
        }
    }
    return 0;
}
