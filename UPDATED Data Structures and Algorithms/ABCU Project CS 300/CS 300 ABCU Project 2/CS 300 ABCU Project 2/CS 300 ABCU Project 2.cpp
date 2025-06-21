// CS 300 ABCU Project 2 – Complete SQLite‐Backed Course Planner


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>           // For std::numeric_limits
#include "sqlite3.h"        // SQLite3 header (must be in ThirdParty\SQLite)

using namespace std;

// -----------------------------------------------------------------------------
// Data Model: a simple struct representing a course in memory (with prerequisites)
struct Course {
    string courseNumber;           // e.g. "CSCI300"
    string courseTitle;            // e.g. "Introduction to Algorithms"
    vector<string> prerequisites;  // e.g. {"CSCI200", "MATH201"}

    Course(const string& num, const string& title)
        : courseNumber(num), courseTitle(title) {}
};

// Global in‐memory list of all courses (populated from SQLite after import)
static vector<Course> courseList;

// -----------------------------------------------------------------------------
// Utility: Check if a given file (by path) exists on disk
bool fileExists(const string& filename) {
    ifstream infile(filename);
    return infile.good();
}

// -----------------------------------------------------------------------------
// 1) Initialize / open the SQLite database ("courses.db") and create tables if needed.
//    Returns true on success (db is open & tables exist), false on failure.
bool initializeDatabase(sqlite3*& db) {
    int rc = sqlite3_open("courses.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return false;
    }

    // Enable foreign key constraints (best practice)
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // Create the Courses table if it does not exist
    const char* createCoursesTbl =
        "CREATE TABLE IF NOT EXISTS Courses ("
        "  course_id   TEXT PRIMARY KEY,"
        "  title       TEXT NOT NULL"
        ");";
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, createCoursesTbl, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error (CREATE Courses): " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    // Create the Prerequisites table if it does not exist
    // Each row says: course_id has prerequisite prereq_id
    const char* createPrereqTbl =
        "CREATE TABLE IF NOT EXISTS Prerequisites ("
        "  course_id   TEXT NOT NULL,"
        "  prereq_id   TEXT NOT NULL,"
        "  PRIMARY KEY(course_id, prereq_id),"
        "  FOREIGN KEY(course_id) REFERENCES Courses(course_id),"
        "  FOREIGN KEY(prereq_id) REFERENCES Courses(course_id)"
        ");";
    rc = sqlite3_exec(db, createPrereqTbl, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error (CREATE Prerequisites): " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// 2) Import from CSV (course_data.csv) into the Courses & Prerequisites tables.
//    Expects CSV lines of the form:
//      CourseNumber,CourseTitle,Prereq1,Prereq2,...
//    If the Courses table is empty, we read every line and:
//      1) INSERT OR IGNORE into Courses(course_id, title).
//      2) For each prereq token, INSERT OR IGNORE into Prerequisites(course_id, prereq_id).
void importCSVtoDB(const string& csvPath, sqlite3* db) {
    ifstream fin(csvPath);
    if (!fin.is_open()) {
        cerr << "Error: Cannot open CSV file: " << csvPath << endl;
        return;
    }

    // Begin an explicit transaction for speed and atomicity
    char* err = nullptr;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &err);
    if (err) {
        cerr << "SQL error (BEGIN): " << err << endl;
        sqlite3_free(err);
    }

    string line;
    // 1) Skip the header line (CourseNumber,CourseTitle,Prerequisites...)
    if (!getline(fin, line)) {
        // File was empty—just roll back and return
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        fin.close();
        return;
    }

    // 2) Process each subsequent non‐empty line
    while (getline(fin, line)) {
        if (line.empty()) continue;

        // Split on commas into tokens
        vector<string> tokens;
        string token;
        stringstream ss(line);
        while (getline(ss, token, ',')) {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            tokens.push_back(token);
        }
        // Need at least courseNumber (tokens[0]) and courseTitle (tokens[1])
        if (tokens.size() < 2) continue;

        string courseID = tokens[0];
        string courseTitle = tokens[1];

        // 3) INSERT OR IGNORE into Courses
        {
            const char* sqlInsertCourse =
                "INSERT OR IGNORE INTO Courses (course_id, title) VALUES (?, ?);";
            sqlite3_stmt* stmtCourse = nullptr;
            sqlite3_prepare_v2(db, sqlInsertCourse, -1, &stmtCourse, nullptr);
            sqlite3_bind_text(stmtCourse, 1, courseID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmtCourse, 2, courseTitle.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmtCourse);
            sqlite3_finalize(stmtCourse);
        }

        // 4) For each token[i], i>=2, insert into Prerequisites(course_id, prereq_id)
        const char* sqlInsertPrereq =
            "INSERT OR IGNORE INTO Prerequisites (course_id, prereq_id) VALUES (?, ?);";
        for (size_t i = 2; i < tokens.size(); ++i) {
            string prereqID = tokens[i];
            if (prereqID.empty()) continue;
            sqlite3_stmt* stmtPrereq = nullptr;
            sqlite3_prepare_v2(db, sqlInsertPrereq, -1, &stmtPrereq, nullptr);
            sqlite3_bind_text(stmtPrereq, 1, courseID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmtPrereq, 2, prereqID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmtPrereq);
            sqlite3_finalize(stmtPrereq);
        }
    }

    // Commit the transaction
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);
    if (err) {
        cerr << "SQL error (COMMIT): " << err << endl;
        sqlite3_free(err);
    }

    fin.close();
    cout << "Imported CSV data into courses.db successfully." << endl;
}

// -----------------------------------------------------------------------------
// 3) Load all courses (and their prerequisites) from SQLite into the in‐memory list.
//    Returns a vector<Course> that mirrors the DB contents.
vector<Course> loadAllCourses(sqlite3* db) {
    vector<Course> courses;
    sqlite3_stmt* stmt = nullptr;

    // 1) Fetch each course’s ID and title
    const char* sqlFetchCourses = "SELECT course_id, title FROM Courses;";
    sqlite3_prepare_v2(db, sqlFetchCourses, -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* idText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* titleText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        Course c(idText ? idText : "", titleText ? titleText : "");
        courses.push_back(c);
    }
    sqlite3_finalize(stmt);

    // 2) For each course, fetch its prereqs
    for (auto& c : courses) {
        const char* sqlFetchPrereqs = "SELECT prereq_id FROM Prerequisites WHERE course_id = ?;";
        sqlite3_prepare_v2(db, sqlFetchPrereqs, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, c.courseNumber.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* prereqText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (prereqText) {
                c.prerequisites.push_back(prereqText);
            }
        }
        sqlite3_finalize(stmt);
    }

    return courses;
}

// -----------------------------------------------------------------------------
// 4) Add a new course (CREATE) via console prompts, insert it into SQLite and memory.
void addCourse(sqlite3* db, vector<Course>& courseList) {
    Course newC("", "");

    cout << "Enter new Course ID: ";
    getline(cin, newC.courseNumber);
    cout << "Enter Course Title: ";
    getline(cin, newC.courseTitle);
    cout << "Enter Prerequisites (comma‐separated, or leave blank): ";
    string prereqLine;
    getline(cin, prereqLine);

    // Parse prereqLine
    {
        stringstream prereqStream(prereqLine);
        string onePrereq;
        while (getline(prereqStream, onePrereq, ',')) {
            // Trim whitespace
            onePrereq.erase(0, onePrereq.find_first_not_of(" \t\r\n"));
            onePrereq.erase(onePrereq.find_last_not_of(" \t\r\n") + 1);
            if (!onePrereq.empty()) {
                newC.prerequisites.push_back(onePrereq);
            }
        }
    }

    // Begin transaction
    char* err = nullptr;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &err);

    // 1) Insert course into Courses table
    {
        const char* sqlInsertCourse =
            "INSERT OR IGNORE INTO Courses (course_id, title) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sqlInsertCourse, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, newC.courseNumber.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, newC.courseTitle.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // 2) Insert each prerequisite into Prerequisites table
    {
        const char* sqlInsertPrereq =
            "INSERT OR IGNORE INTO Prerequisites (course_id, prereq_id) VALUES (?, ?);";
        for (auto& p : newC.prerequisites) {
            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, sqlInsertPrereq, -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, newC.courseNumber.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, p.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Commit transaction
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);
    if (err) {
        cerr << "SQL error (addCourse COMMIT): " << err << endl;
        sqlite3_free(err);
    }

    // Update in‐memory list
    courseList.push_back(newC);
    cout << "Course added successfully." << endl;
}

// -----------------------------------------------------------------------------
// 5) Update an existing course’s title and/or prerequisites (UPDATE), then refresh memory.
void updateCourse(sqlite3* db, vector<Course>& courseList) {
    cout << "Enter Course ID to update: ";
    string courseID;
    getline(cin, courseID);

    // Verify existence in memory
    bool found = false;
    for (const auto& c : courseList) {
        if (c.courseNumber == courseID) {
            found = true;
            break;
        }
    }
    if (!found) {
        cout << "Course ID not found." << endl;
        return;
    }

    cout << "Enter new Title (or leave blank to keep current): ";
    string newTitle;
    getline(cin, newTitle);

    cout << "Enter new Prerequisites (comma‐separated, or leave blank to keep current): ";
    string prereqLine;
    getline(cin, prereqLine);

    // Begin transaction
    char* err = nullptr;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &err);

    // 1) Update title if provided
    if (!newTitle.empty()) {
        const char* sqlUpdateTitle =
            "UPDATE Courses SET title = ? WHERE course_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sqlUpdateTitle, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, newTitle.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, courseID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // 2) Update prerequisites if provided
    if (!prereqLine.empty()) {
        // a) Delete old prereqs for this course
        {
            const char* sqlDelOld = "DELETE FROM Prerequisites WHERE course_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, sqlDelOld, -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, courseID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        // b) Insert new prereqs
        {
            stringstream prereqStream(prereqLine);
            string onePrereq;
            const char* sqlInsertPrereq =
                "INSERT OR IGNORE INTO Prerequisites (course_id, prereq_id) VALUES (?, ?);";
            while (getline(prereqStream, onePrereq, ',')) {
                onePrereq.erase(0, onePrereq.find_first_not_of(" \t\r\n"));
                onePrereq.erase(onePrereq.find_last_not_of(" \t\r\n") + 1);
                if (onePrereq.empty()) continue;
                sqlite3_stmt* stmt = nullptr;
                sqlite3_prepare_v2(db, sqlInsertPrereq, -1, &stmt, nullptr);
                sqlite3_bind_text(stmt, 1, courseID.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, onePrereq.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }
    }

    // Commit transaction
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);
    if (err) {
        cerr << "SQL error (updateCourse COMMIT): " << err << endl;
        sqlite3_free(err);
    }

    // Reload the in‐memory list from the database
    courseList = loadAllCourses(db);
    cout << "Course updated successfully." << endl;
}

// -----------------------------------------------------------------------------
// 6) Delete a course (DELETE) from both Courses and Prerequisites, then refresh memory.
void deleteCourse(sqlite3* db, vector<Course>& courseList) {
    cout << "Enter Course ID to delete: ";
    string courseID;
    getline(cin, courseID);

    // Verify existence in memory
    bool found = false;
    for (const auto& c : courseList) {
        if (c.courseNumber == courseID) {
            found = true;
            break;
        }
    }
    if (!found) {
        cout << "Course ID not found." << endl;
        return;
    }

    // Begin transaction
    char* err = nullptr;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &err);

    // 1) Delete from Prerequisites where course_id = courseID OR prereq_id = courseID
    {
        const char* sqlDelRefs =
            "DELETE FROM Prerequisites WHERE course_id = ? OR prereq_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sqlDelRefs, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, courseID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, courseID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // 2) Delete from Courses where course_id = courseID
    {
        const char* sqlDelCourse = "DELETE FROM Courses WHERE course_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sqlDelCourse, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, courseID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Commit transaction
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);
    if (err) {
        cerr << "SQL error (deleteCourse COMMIT): " << err << endl;
        sqlite3_free(err);
    }

    // Reload in‐memory list
    courseList = loadAllCourses(db);
    cout << "Course deleted successfully." << endl;
}

// -----------------------------------------------------------------------------
// 7) Load Data Structure: Open/init DB, import CSV if Courses table is empty, then load into memory
void loadDataStructure(const string& filename = "") {
    sqlite3* db = nullptr;
    if (!initializeDatabase(db)) {
        cerr << "Failed to initialize database; data structure not loaded." << endl;
        return;
    }

    // Check if the Courses table is empty
    bool needImport = false;
    {
        const char* countSql = "SELECT COUNT(*) FROM Courses;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, countSql, -1, &stmt, nullptr);
        int rc = sqlite3_step(stmt);
        int rowCount = 0;
        if (rc == SQLITE_ROW) {
            rowCount = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        if (rowCount == 0) {
            needImport = true;
        }
    }

    if (needImport) {
        string csvToLoad = filename.empty() ? "course_data.csv" : filename;
        if (!fileExists(csvToLoad)) {
            cerr << "Error: CSV file does not exist: " << csvToLoad << endl;
            sqlite3_close(db);
            return;
        }
        importCSVtoDB(csvToLoad, db);
    }

    // Load all courses and prerequisites into the global in‐memory list
    courseList = loadAllCourses(db);

    sqlite3_close(db);
    cout << "Data structure loaded successfully from SQLite." << endl;
}

// -----------------------------------------------------------------------------
// 8) Print Course List (READ) from the in‐memory list
void printCourseList() {
    cout << "Here is the course list:" << endl;
    for (const auto& course : courseList) {
        cout << course.courseNumber << ", " << course.courseTitle << endl;
    }
}

// -----------------------------------------------------------------------------
// 9) Print a single course’s details (READ) from the in‐memory list
void printCourseInformation(const string& courseNumber) {
    bool found = false;
    for (const auto& course : courseList) {
        if (course.courseNumber == courseNumber) {
            found = true;
            cout << course.courseNumber << ", " << course.courseTitle << endl;
            if (!course.prerequisites.empty()) {
                cout << "Prerequisites:";
                for (const auto& p : course.prerequisites) {
                    cout << " " << p;
                }
                cout << endl;
            }
            else {
                cout << "No prerequisites for this course." << endl;
            }
            break;
        }
    }
    if (!found) {
        cout << "Course not found." << endl;
    }
}

// -----------------------------------------------------------------------------
// 10) A small helper to clear the input buffer when switching between >> and getline()
void ignoreLine() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

// -----------------------------------------------------------------------------
// 11) Console menus for Add, Update, and Delete all call the above functions:

void addCourseMenu(sqlite3* db) {
    addCourse(db, courseList);
}

void updateCourseMenu(sqlite3* db) {
    updateCourse(db, courseList);
}

void deleteCourseMenu(sqlite3* db) {
    deleteCourse(db, courseList);
}

// -----------------------------------------------------------------------------
// 12) Main: display menu, capture user choice, dispatch to functions
int main() {
    int choice = 0;
    string filename, courseNumber;

    do {
        cout << "\n1. Load Data Structure." << endl;
        cout << "2. Print Course List." << endl;
        cout << "3. Print Course." << endl;
        cout << "4. Add Course." << endl;
        cout << "5. Update Course." << endl;
        cout << "6. Delete Course." << endl;
        cout << "9. Exit" << endl;
        cout << "What would you like to do? ";
        cin >> choice;
        ignoreLine();  // clear newline before any getline()

        switch (choice) {
        case 1:
            cout << "Load Data Structure selected." << endl;
            cout << "Enter filename (press Enter for default 'course_data.csv'): ";
            getline(cin, filename);
            loadDataStructure(filename);
            break;

        case 2:
            cout << "Print Course List selected." << endl;
            printCourseList();
            break;

        case 3:
            cout << "Print Course selected." << endl;
            cout << "What course do you want to know about? ";
            getline(cin, courseNumber);
            printCourseInformation(courseNumber);
            break;

        case 4:
            cout << "Add Course selected." << endl;
            addCourseMenu(nullptr); // Placeholder—reopen DB inside addCourseMenu
            break;

        case 5:
            cout << "Update Course selected." << endl;
            updateCourseMenu(nullptr);
            break;

        case 6:
            cout << "Delete Course selected." << endl;
            deleteCourseMenu(nullptr);
            break;

        case 9:
            cout << "Thank you for using the course planner!" << endl;
            break;

        default:
            cout << choice << " is not a valid option." << endl;
            break;
        }
    } while (choice != 9);

    return 0;
}
