int q(int a)
{
    int b = 8;
    for(int m = 0; m < 10; 1)
    {
        b = b + a;
    }
    return b;
}

int test()
{
    return 10;
    return 20;
}
int f(int a)
{
    if(a > 1) { return a * f(a - 1); }
    else { return a; }
}
int n(int a)
{
    return a + 1;
}

int p()
{
    return 5;
}

int main()
{
    n(4);
}
//    std::string s("int f()"
//                  "{"
//                  "for(int a = 1; a < 10; a = a + 2)"
//                  "{"
//                  "a = a + 3;"
//                  "}"
//                  "}");
// std::string s("int f(int b){ b > 2; }");