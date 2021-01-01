interface Foo<T> {
    fun<T: Any> genericFunction1() = 41
    fun<T: Pair<String,Pair<Int, Double>>> genericFunction2() = 42
    fun <T> genericFunction3() = 43
}

class Bar<T, U>(val a: T, val b: U): Foo<T>
class Baz <T, U: Pair<String,Pair<Int, Double>>(val c: T, val d: U): Foo<T>
