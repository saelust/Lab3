# Симуляция транзакций банка

Консольная симуляция экосистемы банковых операций

```c++
			1 Create checking
			2 Deposit
			3 Withdraw
			4 Transfer
			5 List accounts
			6 Print account
			7 Undo last
			0 Exit
```

# Релиз

https://github.com/saelust/Lab3/releases/tag/End

# Функции

<h1>Create checking</h1>

создаёт расчётный счёт (Checking).

```c++
Owner: — имя владельца (строка, можно с пробелами; вводится после строки ввода).

Initial, overdraft: — два числа: начальный баланс (double).
Пример: Initial 100 - значит начальный баланс 100
```
