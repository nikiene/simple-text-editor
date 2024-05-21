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
    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/fb09c8db-09cc-477d-b702-e8eb276afe30)
  
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
    
    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/39ba83b8-2b22-4177-bf31-40af592373e3)  

    It compiles the code and generates an executable.

    After you have succesfully compiled the code just type in this command to create a new file:
    ```
    ./main
    ```
    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/4e766c3a-7815-446e-b52e-6d860b0eb8af)
    
    Or you can specify the path to the file you want to open:

    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/842c303e-128f-47c4-9f4b-d3cd68d0a935)

    ![image](https://github.com/nikiene/simple-text-editor/assets/80795579/0131c1e7-079d-4891-bc2f-3057cee172a7)



#
# INSPIRATIONS:
https://viewsourcecode.org/snaptoken/kilo/01.setup.html 

https://www.averylaird.com/programming/the%20text%20editor/2017/09/30/the-piece-table
#
