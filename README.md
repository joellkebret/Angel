vCard Manager Project
=====================

Overview
--------

The **vCard Manager Project** is a text-based application designed to manage vCard files. It allows users to view, edit, and create vCard entries through an interactive command-line interface. The project combines a high-performance C backend for parsing and validating vCard files with a Python-based frontend for user interaction, all running within a Docker container to ensure a consistent environment. Additionally, it integrates with a MySQL database to store and query vCard data, providing a robust tool for contact management.

Technologies Used
-----------------

This project leverages the following technologies:

*   **Docker**: Ensures a consistent development and runtime environment by encapsulating all dependencies within a container.
    
*   **C**: Handles backend logic, including parsing vCard files (createCard), validating them (validateCard), and writing them to disk (writeCard). The C code is compiled into a shared library (libvcparser.so).
    
*   **Python**: Manages the frontend, coordinating user interaction, the C library, and database operations.
    
*   **Asciimatics**: A Python library that creates a text-based, pseudo-GUI in the terminal for interactive navigation and display of vCard data.
    
*   **Ctypes**: A Python module that enables seamless communication between Python and the C shared library.
    

Prerequisites
-------------

To run this project, ensure you have:

*   **Docker**: Installed and running on your system to provide the containerized environment.
    
*   **SoCS MySQL Database Access**: Valid credentials (username, password, and database name) for connecting to the dursley.socs.uoguelph.ca MySQL server.
    

Setup and Running the Application
---------------------------------

Follow these steps to set up and run the vCard Manager:

1.  **Build the Parser Library**:
    
    *   Open a terminal and navigate to the base directory of the project (e.g., assign3/).
        
    *   Run the following command to compile the C code into a shared library:bashCollapseWrapCopymake parser
        
    *   This generates libvcparser.so in the bin/ directory.
        
2.  **Run the Application**:
    
    *   Navigate to the bin/ directory:bashCollapseWrapCopycd bin
        
    *   Launch the Python application:bashCollapseWrapCopypython3 A3main.py
        
    *   **Note**: These commands should be executed inside the Docker container (see **Notes** below for Docker setup details).
        
3.  **Database Login**:
    
    *   Upon startup, the application displays a **Login View** prompting for:
        
        *   Username (e.g., your SoCS login ID)
            
        *   Password (e.g., your 7-digit student ID)
            
        *   Database name (typically your username)
            
    *   Press **OK** to connect to the SoCS MySQL server (dursley.socs.uoguelph.ca). If the connection fails, an error message will allow you to retry or **Cancel**. Canceling skips database functionality and proceeds to the main interface.
        

What It Does
------------

The vCard Manager provides a powerful set of features for managing vCard files through a text-based interface:

### Main Features

*   **vCard List View (Main View)**:
    
    *   Displays a list of valid vCard files from the cards/ directory (located in bin/).
        
    *   Users can navigate the list using cursor keys and select options:
        
        *   **Create**: Opens an empty vCard Details View to create a new vCard.
            
        *   **Edit**: Opens the vCard Details View for the selected file.
            
        *   **DB Queries**: Accesses the DB Query View (if logged into the database).
            
        *   **Exit**: Closes the application.
            
*   **vCard Details View**:
    
    *   Shows detailed information about a selected vCard, including:
        
        *   File name
            
        *   Contact name (FN property)
            
        *   Birthday (if present)
            
        *   Anniversary (if present)
            
        *   Number of other properties
            
    *   Allows editing of the contact name. Changes are validated and saved to the file and database (if connected) upon pressing **OK**. Pressing **Cancel** discards changes.
        
    *   For new vCards, users can specify a file name and contact name, which are validated and saved.
        
*   **Database Functionality**:
    
    *   Automatically creates and populates two tables in the MySQL database:
        
        *   **FILE**: Stores file metadata (e.g., file\_id, file\_name, last\_modified, creation\_time).
            
        *   **CONTACT**: Stores contact details (e.g., contact\_id, name, birthday, anniversary, file\_id).
            
    *   Updates tables when vCards are created or edited.
        
    *   Offers a **DB Query View** with two predefined queries:
        
        *   **Display all contacts**: Lists all contacts with file names, sorted by name or file name.
            
        *   **Find contacts born in June**: Shows names and birthdays of contacts born in June, sorted by age.
            

### Workflow

1.  On startup, the app scans the cards/ directory, validates vCard files using the C library, and populates the Main View and database (if connected).
    
2.  Users interact with the text-based UI using cursor and tab keys to navigate and select options.
    
3.  Changes to vCards are reflected on disk and in the database, ensuring data consistency.
    

Notes
-----

*   **Docker Requirement**: The application must run within the provided Docker container to ensure compatibility with pre-installed libraries (e.g., Asciimatics). To set up:
    
    *   Pull the CIS\*2750 Docker image (refer to course materials for the exact image name and instructions).
        
    *   Run the container and mount the project directory, e.g.:bashCollapseWrapCopydocker run -it -v /path/to/project:/app cis2750-image
        
    *   Execute the make parser and python3 A3main.py commands inside the container.
        
*   **Cards Directory**: Ensure the cards/ directory exists in bin/ and contains valid .vcf or .vcard files for the application to process.
    
*   **Database Credentials**: Do not hardcode credentials; they must be entered via the Login View at runtime.
    
*   **Error Handling**: The app gracefully handles invalid inputs and database connection issues, prompting retries where applicable.
    
