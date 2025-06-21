#include <iostream>        
// Include the input/output stream library
#include <fstream>         
// Include the file stream library
#include <sstream>         
// Include the string stream library
#include <string>          
// Include the string library
#include <vector>          
// Include the vector library
#include <algorithm>       
// Include the algorithm library

using namespace std;      
// Use the standard namespace

// Define a struct to represent a course
struct Course {
    std::string courseNumber;                   
    // Define a string to hold course number
    std::string courseTitle;                    
    // Define a string to hold course title
    std::vector<std::string> prerequisites;     
    // Define a vector to hold prerequisites

    Course(std::string num, std::string title) : courseNumber(num), courseTitle(title) {}  // Constructor to initialize course with number and title
};

// Global vector to store all course objects
std::vector<Course> courseList;                
// Define a vector to store Course objects globally

// Function to check if a file exists
bool fileExists(const std::string& filename) {
    std::ifstream infile(filename);            
    // Open file stream with given filename
    return infile.good();                       
    // Return true if file exists, false otherwise
}

// Function prototypes
void loadDataStructure(const std::string& filename = "");   
// Prototype for function to load data structure from file
void printCourseList();                                      
// Prototype for function to print course list
void printCourseInformation(const std::string& courseNumber); 
// Prototype for function to print course information based on course number

int main() {
    std::string courseNumber;            
    // Declare a string to hold course number
    int choice;                          
    // Declare an integer to hold user's choice
    std::string filename;                
    // Declare a string to hold filename for data loading

    do {
        // Display menu options
        std::cout << "\n1. Load Data Structure." << std::endl;
        std::cout << "2. Print Course List." << std::endl;
        std::cout << "3. Print Course." << std::endl;
        std::cout << "9. Exit" << std::endl;
        std::cout << "What would you like to do? ";
        std::cin >> choice;                  
        // Read user's choice

        // Perform action based on user choice
        switch (choice) {
        case 1:
            std::cout << "Load Data Structure selected." << std::endl;
            // Prompt the user to input the filename
            std::cout << "Enter filename (press Enter for default 'course_data.csv'): ";
            std::cin.ignore();                
            // Clear input buffer
            std::getline(std::cin, filename); 
            // Read filename from user
            loadDataStructure(filename.empty() ? "course_data.csv" : filename);  // Load data structure from file
            break;
        case 2:
            std::cout << "Print Course List selected." << std::endl;
            printCourseList();                
            // Print course list
            break;
        case 3:
            std::cout << "Print Course selected." << std::endl;
            // Ask for course number and print course information
            std::cout << "What course do you want to know about? ";
            std::cin >> courseNumber;         
            // Read course number from user
            printCourseInformation(courseNumber); 
            // Print course information based on course number
            break;
        case 9:
            std::cout << "Thank you for using the course planner!" << std::endl;
            break;
        default:
            std::cout << choice << " is not a valid option." << std::endl; // Display error message for invalid choice
        }
    } while (choice != 9);                      
    // Continue loop until user chooses to exit

    return 0;                                    
    // Return 0 to indicate successful execution
}

// Function to load data from file into data structure
void loadDataStructure(const std::string& filename) {
    // Clear existing course list
    courseList.clear();

    // Check if the file exists
    if (!fileExists(filename)) {                   
        // Check if file exists
        std::cerr << "Error: File does not exist at path " << filename << std::endl; // Display error message if file does not exist
        return;                                   
        // Return from function
    }

    // Open the file
    std::ifstream file(filename);                
    // Open file stream with given filename
    if (!file.is_open()) {                        
        // Check if file is opened successfully
        std::cerr << "Error: Failed to open file " << filename << std::endl; // Display error message if file cannot be opened
        return;                                  
        // Return from function
    }

    // Read each line from the file
    std::string line;                            
    // Declare string to hold each line from file
    while (std::getline(file, line)) {           
        // Read each line from file
        std::istringstream iss(line);            
        // Create stringstream from line
        std::string courseNumber, courseTitle, prereqString; 
        // Declare strings to hold course details

        // Split the line by commas
        std::getline(iss, courseNumber, ',');    
        // Get course number from stringstream
        std::getline(iss, courseTitle, ',');     
        // Get course title from stringstream
        std::getline(iss, prereqString);         
        // Get prerequisites from stringstream

        // Remove leading and trailing whitespaces from courseTitle and prereqString
        courseTitle.erase(courseTitle.find_last_not_of(" \n\r\t") + 1); 
        // Remove trailing whitespaces
        prereqString.erase(prereqString.find_last_not_of(" \n\r\t") + 1);
        // Remove trailing whitespaces

        Course course(courseNumber, courseTitle); 
        // Create Course object with course number and title

        // Parse prerequisites if available
        if (prereqString != "None") {             
            // Check if prerequisites are provided
            std::istringstream prereqStream(prereqString); 
            // Create stringstream from prerequisites string
            std::string prereq;                  
            // Declare string to hold each prerequisite
            while (std::getline(prereqStream, prereq, ',')) { 
                // Read each prerequisite from stringstream
                course.prerequisites.push_back(prereq); 
                // Add prerequisite to course object
            }
        }

        // Add course to the list
        courseList.push_back(course);            
        // Add course object to global course list
    }

    std::cout << "Data loaded successfully." << std::endl; 
    // Display success message
}

// Function to print an alphanumeric list of all courses
void printCourseList() {
    std::cout << "Here is a sample schedule:" << std::endl; 
    // Display message
    for (const auto& course : courseList) {             
        // Iterate through each course in course list
        std::cout << course.courseNumber << ", " << course.courseTitle << std::endl; // Print course number and title
    }
}

// Function to print course information based on course number
void printCourseInformation(const std::string& courseNumber) {
    bool found = false;                              
    // Declare boolean variable to track if course is found
    for (const auto& course : courseList) {          
        // Iterate through each course in course list
        if (course.courseNumber == courseNumber) {   
            // Check if course number matches given course number
            found = true;                            
            // Set found flag to true
            std::cout << course.courseNumber << ", " << course.courseTitle << std::endl; // Print course number and title
            if (!course.prerequisites.empty()) {     
                // Check if course has prerequisites
                std::cout << "Prerequisites:";      
                // Display message
                for (const auto& prereq : course.prerequisites) { 
                    // Iterate through each prerequisite
                    std::cout << " " << prereq;     
                    // Print prerequisite
                }
                std::cout << std::endl;             
                // Move to next line
            }
            else {
                std::cout << "No prerequisites for this course." << std::endl; // Display message if no prerequisites
            }
            break;                                 
            // Exit loop
        }
    }
    if (!found) {                                   
        // Check if course was not found
        std::cout << "Course not found." << std::endl; 
        // Display error message
    }
}
