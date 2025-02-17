#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>

struct User {
    int userID;
    char name[30];
    char email[50];
    int firstReviewIndex;
    int reviewCount;
    bool isDeleted;
};

struct Review {
    int reviewID;
    int userID;
    char content[100];
    int nextReviewIndex;
    bool isDeleted;
};

struct UserIndex {
    int userID;
    int recordIndex;
};

std::vector<int> userTrash; // Сміттєва зона для видалених записів користувачів
std::vector<int> reviewTrash; // Сміттєва зона для видалених записів відгуків

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

void getMaster(int userID) {
    std::ifstream indexFile("users.ind", std::ios::binary);
    UserIndex index;
    while (indexFile.read(reinterpret_cast<char*>(&index), sizeof(UserIndex))) {
        if (index.userID == userID) {
            std::ifstream masterFile("users.fl", std::ios::binary);
            masterFile.seekg(index.recordIndex * sizeof(User));
            User user;
            masterFile.read(reinterpret_cast<char*>(&user), sizeof(User));
            if (!user.isDeleted) {
                std::cout << "UserID: " << user.userID << "\nName: " << user.name << "\nEmail: " << user.email << "\nReviewCount: " << user.reviewCount << std::endl;
            } else {
                std::cout << "Record is deleted." << std::endl;
            }
            masterFile.close();
            return;
        }
    }
    std::cout << "User not found." << std::endl;
    indexFile.close();
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

    void delSlave(int reviewIndex);
    
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
    for (const auto& idx : indexTable) {
        indexFileOut.write(reinterpret_cast<const char*>(&idx), sizeof(UserIndex));
    }
    indexFileOut.close();
}
    

void delSlave(int reviewIndex) {
    std::fstream reviewFile("reviews.fl", std::ios::binary | std::ios::in | std::ios::out);
    Review review;
    reviewFile.seekg(reviewIndex * sizeof(Review));
    reviewFile.read(reinterpret_cast<char*>(&review), sizeof(Review));
    review.isDeleted = true;
    reviewFile.seekp(reviewIndex * sizeof(Review));
    reviewFile.write(reinterpret_cast<char*>(&review), sizeof(Review));
    reviewFile.close();

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

    if (field == "name") {
        std::cout << "Enter new name: ";
        std::cin.ignore();
        std::cin.getline(user.name, 30);
    } else if (field == "email") {
        std::cout << "Enter new email: ";
        std::cin.ignore();
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

    if (field == "content") {
        std::cout << "Enter new content: ";
        std::cin.ignore();
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
        if (!user.isDeleted) {
            activeCount++;
        }
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
        if (user.isDeleted) {
            continue;
        }

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
        std::cout << "Index: " << index << " | UserID: " << user.userID << " | Name: " << user.name << " | Email: " << user.email
                  << " | FirstReviewIndex: " << user.firstReviewIndex << " | ReviewCount: " << user.reviewCount
                  << " | isDeleted: " << user.isDeleted << std::endl;
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
        std::cout << "Index: " << index << " | ReviewID: " << review.reviewID << " | UserID: " << review.userID << " | Content: " << review.content
                  << " | NextReviewIndex: " << review.nextReviewIndex << " | isDeleted: " << review.isDeleted << std::endl;
        ++index;
    }

    slaveFile.close();
}

int main() {
    createMaster();
    createSlave();

    std::string command;
    int id;

    while (true) {
        std::cout << "\nEnter command (get-m, get-s, del-m, del-s, insert-m, insert-s, update-m, update-s, calc-m, calc-s, ut-m, ut-s, exit): ";
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
        } else if (command == "exit") {
            break;
        } else {
            std::cout << "Unknown command!" << std::endl;
        }
    }

    return 0;
}
