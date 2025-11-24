// mini_bank_no_overdraft.cpp
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace std::chrono;

struct BankError : runtime_error { using runtime_error::runtime_error; };
struct NotFound : BankError { using BankError::BankError; };
struct Insufficient : BankError { using BankError::BankError; };

enum class TxType { Deposit, Withdraw, TransferIn, TransferOut };

struct Tx {
	int acc;
	TxType type;
	double amt;
	string note;
	time_t when;
	Tx(int a, TxType t, double v, string n = "")
		: acc(a), type(t), amt(v), note(move(n)), when(time(nullptr)) {
	}
};

struct Account {
	int id;
	string owner;
	double bal = 0.0;
	vector<Tx> hist;

	Account(int i, string o, double init = 0.0)
		: id(i), owner(move(o)), bal(init) {
		if (init > 0) hist.emplace_back(id, TxType::Deposit, init, "initial");
	}

	virtual ~Account() = default;

	virtual void deposit(double v, string note = "") {
		if (v <= 0) throw BankError("deposit>0");
		bal += v;
		hist.emplace_back(id, TxType::Deposit, v, note);
	}

	virtual void withdraw(double v, string note = "") {
		if (v <= 0) throw BankError("withdraw>0");
		if (bal < v) throw Insufficient("insufficient");
		bal -= v;
		hist.emplace_back(id, TxType::Withdraw, v, note);
	}

	void print() const {
		cout << "Account " << id << " (" << owner << ") balance=" << fixed << setprecision(2) << bal << '\n';
		for (auto const& t : hist) {
			tm tmv;
#if defined(_MSC_VER)
			localtime_s(&tmv, &t.when);
#else
			tm* p = localtime(&t.when);
			tmv = p ? *p : tm{};
#endif
			char buf[32];
			strftime(buf, sizeof(buf), "%F %T", &tmv);
			string typ;
			switch (t.type) {
			case TxType::Deposit:
				typ = "Dep +";
				break;
			case TxType::Withdraw:
				typ = "Wdr -";
				break;
			case TxType::TransferIn:
				typ = "TIn +";
				break;
			case TxType::TransferOut:
				typ = "TOut -";
				break;
			}
			cout << "  [" << buf << "] " << typ << ' ' << fixed << setprecision(2) << t.amt << " | " << t.note << '\n';
		}
	}
};

struct Checking : Account {
	Checking(int i, string o, double init)
		: Account(i, o, init) {
	}
	// No overdraft — use base withdraw behavior
};

struct Bank {
	unordered_map<int, unique_ptr<Account>> a;
	vector<Tx> log; // global log for undo
	int next = 1;

	int createChecking(string owner, double init = 0) {
		int id = next++;
		a[id] = make_unique<Checking>(id, owner, init);
		if (init > 0) log.emplace_back(id, TxType::Deposit, init, "initial");
		return id;
	}

	Account& acc(int id) {
		auto it = a.find(id);
		if (it == a.end()) throw NotFound("no account");
		return *it->second;
	}

	void deposit(int id, double v, string note = "") {
		acc(id).deposit(v, note);
		log.emplace_back(id, TxType::Deposit, v, note);
	}

	void withdraw(int id, double v, string note = "") {
		acc(id).withdraw(v, note);
		log.emplace_back(id, TxType::Withdraw, v, note);
	}

	void transfer(int from, int to, double v, string note = "") {
		if (from == to) throw BankError("same account");
		acc(from).withdraw(v, "to " + to_string(to) + (note.size() ? ": " + note : ""));
		acc(to).deposit(v, "from " + to_string(from) + (note.size() ? ": " + note : ""));
		log.emplace_back(from, TxType::TransferOut, v, "to " + to_string(to));
		log.emplace_back(to, TxType::TransferIn, v, "from " + to_string(from));
	}

	void list() {
		cout << "Accounts:\n";
		for (auto& p : a)
			cout << " id=" << p.first << " owner=" << p.second->owner << " bal=" << fixed << setprecision(2) << p.second->bal << '\n';
	}

	bool undo(string& err) {
		if (log.empty()) { err = "nothing to undo"; return false; }
		Tx last = log.back();
		// check transfer pair
		if (last.type == TxType::TransferIn && log.size() >= 2) {
			Tx prev = log[log.size() - 2];
			if (prev.type == TxType::TransferOut && prev.amt == last.amt) {
				int to = last.acc, from = prev.acc;
				double v = last.amt;
				try {
					acc(to).withdraw(v, "undo transfer");
					acc(from).deposit(v, "undo transfer");
					log.pop_back();
					log.pop_back();
					return true;
				}
				catch (exception& e) { err = e.what(); return false; }
			}
		}
		// single undo
		try {
			if (last.type == TxType::Deposit || last.type == TxType::TransferIn) {
				acc(last.acc).withdraw(last.amt, "undo");
			}
			else {
				acc(last.acc).deposit(last.amt, "undo");
			}
			log.pop_back();
			return true;
		}
		catch (exception& e) { err = e.what(); return false; }
	}
};

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	Bank bank;
	cout << "Mini bank (no overdraft) interactive\n";

	while (true) {
		cout << "\nMenu:\n"
			<< "\t1 Create checking\n"
			<< "\t2 Deposit\n"
			<< "\t3 Withdraw\n"
			<< "\t4 Transfer\n"
			<< "\t5 List accounts\n"
			<< "\t6 Print account\n"
			<< "\t7 Undo last\n"
			<< "\t0 Exit\n"
			<< "Choose: ";
		int cmd;
		if (!(cin >> cmd)) break;

		try {
			if (cmd == 1) {
				string name;
				double init;
				cout << "Owner: ";
				cin >> ws;
				getline(cin, name);
				cout << "Initial: ";
				cin >> init;
				int id = bank.createChecking(name, init);
				cout << "Created checking id=" << id << "\n";
			}
			else if (cmd == 2) {
				int id;
				double v;
				cout << "acc id, amount: ";
				cin >> id >> v;
				bank.deposit(id, v, "manual");
				cout << "OK\n";
			}
			else if (cmd == 3) {
				int id;
				double v;
				cout << "acc id, amount: ";
				cin >> id >> v;
				bank.withdraw(id, v, "manual");
				cout << "OK\n";
			}
			else if (cmd == 4) {
				int a, b;
				double v;
				cout << "from to amount: ";
				cin >> a >> b >> v;
				bank.transfer(a, b, v, "manual");
				cout << "OK\n";
			}
			else if (cmd == 5) {
				bank.list();
			}
			else if (cmd == 6) {
				int id;
				cout << "acc id: ";
				cin >> id;
				bank.acc(id).print();
			}
			else if (cmd == 7) {
				string err;
				if (bank.undo(err)) cout << "Undo ok\n";
				else cout << "Undo failed: " << err << '\n';
			}
			else if (cmd == 0) {
				cout << "Bye\n";
				break;
			}
			else {
				cout << "Unknown\n";
			}
		}
		catch (exception& e) {
			cout << "Error: " << e.what() << "\n";
		}
	}

	return 0;
}
