# simple-text-editor
We finally have a functional editor, were you can open a new or existing file, modify it, and save it to the disk!
I still need to add more functions to it, but im happy to say that you can try it already.

# Running the editor:

* Clone the repository:

    First of all we need to clone the git repository on our local machine.
    Fire up your prefered terminal and type in
    ```
    git clone https://github.com/nikiene/simple-text-editor.git
    ```
* Get inside the project's folder:

    ```
    cd ./simple-text-editor
    ```
* Setup:
  
  Install the GCC C Compiler and the Make program.
  
  For Windows users:
  
    You will need to install some kind of Linux environment on your machine. You can use a Virtual Machine or WSL (Official Microsoft WSL guide: https://learn.microsoft.com/pt-br/windows/wsl/about).
    After installing your preferred Linux distribution, you will need to run this command inside the bash: 
    ```
    bash
    sudo apt-get install gcc make
    ```
    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/7c09f6f3-644c-43d8-bb2a-a08b83e48de6)
  
    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/f682706d-d271-4af6-b801-39debbb625b4)


  For Mac users:
  
    Run this on your terminal:
    ```
    xcode-select --install
    ```
  For Linux users:
  
    Run this on your terminal:
    ```
    sudo apt-get install gcc make
    ```

  Now that everything is set up, we can actually run the program!

* Running:

  To run the editor is very simple:

    Inside the project folder just type in this command:
    ```
    make
    ```
    
    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/bfc831ce-9168-40b4-ab99-deaa5587c857)
  

    It compiles the code and generates an executable.

    After you have succesfully compiled the code just type in this command to create a new file:
    ```
    ./main
    ```
    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/2a5a99db-70e0-4490-ad6f-1d6271452c48)
    
    Or you can specify the path to the file you want to open:

    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/f8f00db3-987c-4d6c-b8f9-b6dc9d2f256f)


#
# INSPIRATIONS:
https://viewsourcecode.org/snaptoken/kilo/01.setup.html 

https://www.averylaird.com/programming/the%20text%20editor/2017/09/30/the-piece-table
#
