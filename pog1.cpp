/*/ hello.cpp file
#include <iostream>
int main()
{
    std::cout << "Hello Geek\n";
    return 0;
}
#include <iostream>
using namespace std;

int main(){
    using std::cout;
    int slices = 5;
    int slice;
    cin >> slice;
    cout << "Hello Workd" << slice << std::endl;
    printf("%i\n", slices);
}
int  power(base,p){
    int i = 1
    int k = 1
    while(int i; i <= p; i++){
        k = k * base
    }
    return k
}

#include <iostream>
#include <cmath>
using std::cout;
using std::cin;

double  power(double base, int exponent)
{
    return 0.0;
}
int main(){
    int base, exponent;
    cout << "base: ";
    cin >>base;
    cout << "exponent: ";
    cin >> exponent;
    cout << pow(base,exponent);
}


int main(){
    std::string greeting;
    std::cin >> greeting;
    std
}
#include <iostream>
#include <string>

int main ()
{
    std::string greeting = "what the hell?";
    //greeting.insert(3,"         ");
    //greeting.pop_back();
    int k = greeting.find("hell");
    greeting.replace(k,4,"****");
    std::cout << greeting << std::endl;
    return 0;
}*/
#include <iostream>
int main(){
    enum class Season{summer,winter,spring,fall};
    Season now = Season::winter;
    switch(now){
        case Season::summer:
            break;
        case Season::spring:
            break;
        case Season::fall:
            break;
        case Season::winter:
            std::cout << "STAY WARM BUDDY" << std::endl;
            break;
    }
    return 0;
}