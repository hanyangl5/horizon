# code style of horizon

## naming rules

both c++ and shader code.

### variable

for general vairables, all lowercase, with underscores between words.

```
int a_b_c = 0;
```

for class member variables, add ```m_``` before the variable name.

```
class A{
    int m_a_b_c = 0;
}
```

for constant variables, add ```k_``` before the variable name.

```
const int k_a_b_c = 0;
```

### function

functions should start with a capital letter and have a capital letter for each new word.

```
void ABC()
{

}
```




