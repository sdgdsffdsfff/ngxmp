arrayv module
        array variable, a variable contains some variables as an array.

commands:

push_array
        push_array $array $elts;
        this cmd means, you add a new element to the array using push(OP_HEAD), $array is the array u want to add,
        and $elts is the variable to be added.

pop_array
        pop_array  $out $array;
        this cmd means delete a element from an array and put the delete element's value to the $out.

ushift_array
        ushift_array $array $elts;
        the same as the push_array, but the add function using OP_TAIL。

shift_array
        shift_array $out $array;
        the same as the pop_array, but the delete OP using OP_TAIL.

print_array
        should take 3 args, it like array to string.
        print_array $out $array format_string;
        use this cmd, the elts of the array will print to $out as the format the format_string defined.
        such as "\{{(\"$:$\"),?}\}". '{','$','(' and '?' are define to be format special use.so ,if your format contais
        these chars, please use '\' to diff.
        "\{{(\"$:$\"),?}\}" means use '{' as head '}' as tail ,2 elts as one new element, and ','to between them. '"'\
        to as the lhead and ltail.
        so if the array has the elts: a,b,c,d,e,f,g
        it will be: {"a:b","c:d","e:f"},'g'will be cut.

str_to_array
        should also take 3 args. put a string to an array use delete string.
        str_to_array $array string del_uchar;
        eg.  str_to_array $array "$a$b" ":";
        it means use ':'to del the string ,and each new string as a new element add to $array. "$a$b" also can
        be just a string, such as str_to_array $array "aaa:ccc:d:ef".
        this add op usd OP_HEAD.

About the http.nginx.conf.
        you can test the arrayv module use this conf, "/rplatform?" will return two {} string.
				if "/rplatform?" with a query_string like "a:b:c" (':'as the del char), it will call 'str_to_array',and
        a newer return value with the new arrayv will output.



any problem please contact with the author: zhangyu4@staff.sina.com.cn .
        
