C++ notes
greeting.size()
greeting[i]
greeting.find_first_of("aeiou")
greeting.find("")
greting .insert(pos, "")
greeting.replace(k,4,"****");
greeting.pop_back();
//commnent
/*comment*/
using std::string;string hello;
std::cout << x << std::endl;
cin.getline()
void hello(){}
greeting.
unsigned long x = -1; //suiigned does not allow neg values
if(greeting.find_first_of("!") == -1) std::cout << "NOT FOUND" << std.endl;
auto x = 5U; //c++ 11 feature, type will be assigned based on what argument you give it
g++ main.cpp -std=c++11
auto x = 5.5L;5.5F,5.L,5.0double]
//3.4rewatch conversion from different bases hexadecimal and octal rewatch at end for memory adresses
//rewatch 2:15
double x = 10. /4;  std::cout << x << std::endl; //need .0 and double to receive 2.5 as answer instwad of 2 need .0
const int x = 5; //revieww constant
if(i<23){}
else if(i>23){}
else{}
switch(age)
{
    case 17:
        //code
        break //you nee break statement else all below code will run, can also use return 0
    case 18:
        //code
        break
    default:
        //code
        break

}
if (!(name_guess ==name_asnswer)) {continue}//!= contiue breaks for loop
/*enum notes
#include <iostream>
int main(){
    enum season{summer,winter,spring,fall};
    season now = winter;
    switch(now){
        case summer:
            break;
        case spring:
            break;
        case fall:
            break;
        case winter:
            std::cout << "STAY WARM BUDDY" << std::endl;
            break;
    }
    return 0;
}*/
//review class enums and diff between using either class enum or normal enum
while(){}
for(int i = 0;i <10;i++){}
\t -> space
factorial *= i;i--
\n -> newline
while(true){}
/*do while - use this next
do{
    std::cout << "password: ";
    std::cin >> guess;
    }while(guess != pssword);
*/
/*
for(int i = 0; i > sentence.size();i++)
{
    if(sentence[i] == '  '){
        continue}
    std::cout << sentence[i] << std::endl
}*/
int points = guess === answer ? 10 : 0; //importatn and interesting
guess == answer ? std::cout << "good job \n" : std::cout << "Bad job\n"; // run this core once in the termminal and check 
//refocus on abote topic after
g++ main.cpp /o name
//make that random game later
int guess[7] //size array staticly sized size is state youcan increase storage

#include <iostream>
int main(){// if size is written they you are defining final size else the amount of sdatq decides it
    int guesses[20] = {10,13,21,34,23,12};// 7 will be always final size
    guesses[3]
    return 0;
}
sizeof(array) /sizeof(guesses[0])//finds size of array rewatchy 5.02
if(std::cin >> guesses[i]){}else{break;}
/// stackoverlfow key 5:19
""->fors tring
''->for character
