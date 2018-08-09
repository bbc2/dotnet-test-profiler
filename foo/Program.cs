using System;

namespace foo
{
    class Program
{ 
        static int foo(int a, int b, int c, string d)
        {
            return a + b + c + d.Length;
        }

        static void Main(string[] args)
        {
            Console.Write("foo: {0:d}\n", foo(0xaa, 0xbb, 0xcc, "bar"));
        }
    }
}
