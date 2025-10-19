#include <bits/stdc++.h>
using namespace std;
using ll = long long;

// ----------------- Utilities: time formatting -----------------
time_t now_time_t() {
    return time(nullptr);
}

string time_t_to_iso(time_t t) {
    char buf[64];
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmv);
    return string(buf);
}

time_t days_ago_seconds(int days) {
    return now_time_t() - (time_t)days * 24 * 3600;
}

int date_key_from_time(time_t t) {
    return (int) (t / 86400);
}

pair<int,int> year_month_from_time(time_t t) {
    struct tm tmv;
    localtime_r(&t, &tmv);
    return {tmv.tm_year + 1900, tmv.tm_mon + 1};
}

// ----------------- Models -----------------
struct BankTx {
    string id;
    string type; 
    double amount;
    time_t ts;
    string note;
};

struct BankAccount {
    string id;
    double balance = 0.0;
    vector<BankTx> txs;

    BankAccount() {
        static int cnt = 1;
        id = "BA" + to_string(cnt++);
    }

    void credit(double amt, const string &note="") {
        balance += amt;
        BankTx tx{ "TX"+to_string((int)txs.size()+1), "credit", amt, now_time_t(), note };
        txs.push_back(tx);
    }
    bool debit(double amt, const string &note="") {
        if (amt > balance) return false;
        balance -= amt;
        BankTx tx{ "TX"+to_string((int)txs.size()+1), "debit", amt, now_time_t(), note };
        txs.push_back(tx);
        return true;
    }
    vector<BankTx> txs_between(time_t start, time_t end) const {
        vector<BankTx> out;
        for (auto &tx: txs) if (tx.ts >= start && tx.ts <= end) out.push_back(tx);
        return out;
    }
};

// forward
struct Item;
struct Transaction;

struct User {
    string id;
    string username;
    bool is_seller;
    BankAccount account;

    User() {}
    User(const string &i, const string &u, bool s): id(i), username(u), is_seller(s) {}
};

struct Item {
    string id;
    string name;
    double price;
    int stock;
    string seller_id;
};

struct Transaction {
    string id;
    string buyer_id;
    string seller_id;
    string item_id;
    int qty;
    double total;
    time_t ts;
    string status; 
};

// ----------------- Store -----------------
struct Store {
    int user_ctr = 0;
    int item_ctr = 0;
    int tx_ctr = 0;

    unordered_map<string, User> users; 
    unordered_map<string, Item> items; 
    unordered_map<string, Transaction> txs; 

    // register user
    string register_user(const string &username, bool is_seller) {
        for (auto &p : users) if (p.second.username == username) throw runtime_error("Username exists");
        user_ctr++;
        string uid = "U" + to_string(user_ctr);
        User u(uid, username, is_seller);
        u.account = BankAccount();
        users[uid] = u;
        return uid;
    }

    string find_user_by_username(const string &username) {
        for (auto &p : users) if (p.second.username == username) return p.first;
        return "";
    }

    // item
    string add_item(const string &seller_id, const string &name, double price, int stock) {
        if (!users.count(seller_id)) throw runtime_error("Seller not found");
        if (!users[seller_id].is_seller) throw runtime_error("User is not seller");
        item_ctr++;
        string iid = "I" + to_string(item_ctr);
        Item it;
        it.id = iid; it.name = name; it.price = price; it.stock = stock; it.seller_id = seller_id;
        items[iid] = it;
        return iid;
    }

    void replenish_item(const string &item_id, int qty) {
        if (!items.count(item_id)) throw runtime_error("Item not found");
        items[item_id].stock += qty;
    }

    void discard_item(const string &item_id, int qty) {
        if (!items.count(item_id)) throw runtime_error("Item not found");
        items[item_id].stock = max(0, items[item_id].stock - qty);
    }

    void set_price(const string &item_id, double price) {
        if (!items.count(item_id)) throw runtime_error("Item not found");
        items[item_id].price = price;
    }

    // orders
    string create_order(const string &buyer_id, const string &item_id, int qty) {
        if (!users.count(buyer_id)) throw runtime_error("Buyer not found");
        if (!items.count(item_id)) throw runtime_error("Item not found");
        if (items[item_id].stock < qty) throw runtime_error("Not enough stock");
        tx_ctr++;
        string txid = "T" + to_string(tx_ctr);
        Transaction t;
        t.id = txid; t.buyer_id = buyer_id; t.seller_id = items[item_id].seller_id;
        t.item_id = item_id; t.qty = qty; t.total = items[item_id].price * qty;
        t.ts = now_time_t(); t.status = "created";
        txs[txid] = t;
        return txid;
    }

    void pay_order(const string &txid) {
        if (!txs.count(txid)) throw runtime_error("Tx not found");
        Transaction &t = txs[txid];
        if (t.status != "created") throw runtime_error("Not in payable state");
        User &buyer = users.at(t.buyer_id);
        User &seller = users.at(t.seller_id);
        if (!buyer.account.debit(t.total, "pay tx " + txid)) throw runtime_error("Insufficient balance");
        seller.account.credit(t.total, "receive tx " + txid);
        items[t.item_id].stock -= t.qty;
        t.status = "paid"; t.ts = now_time_t();
    }

    void complete_order(const string &txid) {
        if (!txs.count(txid)) throw runtime_error("Tx not found");
        Transaction &t = txs[txid];
        if (t.status != "paid") throw runtime_error("Only paid orders can be completed");
        t.status = "complete"; t.ts = now_time_t();
    }

    void cancel_order(const string &txid) {
        if (!txs.count(txid)) throw runtime_error("Tx not found");
        Transaction &t = txs[txid];
        if (t.status == "paid") {
            User &buyer = users.at(t.buyer_id);
            User &seller = users.at(t.seller_id);
            if (!seller.account.debit(t.total, "refund " + txid)) throw runtime_error("Seller has insufficient funds to refund");
            buyer.account.credit(t.total, "refund " + txid);
        }
        t.status = "cancelled"; t.ts = now_time_t();
    }

    // Reports
    vector<Transaction> transactions_latest_k_days(int k) {
        time_t cutoff = days_ago_seconds(k);
        vector<Transaction> out;
        for (auto &p : txs) if (p.second.ts >= cutoff) out.push_back(p.second);
        sort(out.begin(), out.end(), [](const Transaction &a, const Transaction &b){ return a.ts > b.ts; });
        return out;
    }

    vector<Transaction> paid_but_not_completed() {
        vector<Transaction> out;
        for (auto &p : txs) if (p.second.status == "paid") out.push_back(p.second);
        return out;
    }

    vector<pair<string,int>> most_frequent_items(int m) {
        unordered_map<string,int> cnt;
        for (auto &p : txs) {
            if (p.second.status == "paid" || p.second.status == "complete") cnt[p.second.item_id] += p.second.qty;
        }
        vector<pair<string,int>> vec;
        for (auto &q: cnt) vec.push_back(q);
        sort(vec.begin(), vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
        if ((int)vec.size() > m) vec.resize(m);
        // convert id->name
        vector<pair<string,int>> out;
        for (auto &pr : vec) {
            string name = items.count(pr.first) ? items[pr.first].name : pr.first;
            out.push_back({name, pr.second});
        }
        return out;
    }

    vector<pair<string,int>> most_active_buyers_on_date(time_t day, int top_n=5) {
        int key = date_key_from_time(day);
        unordered_map<string,int> cnt;
        for (auto &p : txs) if (date_key_from_time(p.second.ts) == key) cnt[p.second.buyer_id]++;
        vector<pair<string,int>> vec;
        for (auto &q: cnt) vec.push_back(q);
        sort(vec.begin(), vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
        vector<pair<string,int>> out;
        for (int i=0;i<(int)vec.size() && i<top_n;i++){
            string name = users.count(vec[i].first) ? users[vec[i].first].username : vec[i].first;
            out.push_back({name, vec[i].second});
        }
        return out;
    }

    vector<pair<string,int>> most_active_sellers_on_date(time_t day, int top_n=5) {
        int key = date_key_from_time(day);
        unordered_map<string,int> cnt;
        for (auto &p : txs) if (date_key_from_time(p.second.ts) == key) cnt[p.second.seller_id]++;
        vector<pair<string,int>> vec;
        for (auto &q: cnt) vec.push_back(q);
        sort(vec.begin(), vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
        vector<pair<string,int>> out;
        for (int i=0;i<(int)vec.size() && i<top_n;i++){
            string name = users.count(vec[i].first) ? users[vec[i].first].username : vec[i].first;
            out.push_back({name, vec[i].second});
        }
        return out;
    }

    vector<pair<string,int>> top_k_popular_items_per_month(int year, int month, int k) {
        unordered_map<string,int> cnt;
        for (auto &p : txs) {
            auto ym = year_month_from_time(p.second.ts);
            if (ym.first == year && ym.second == month) cnt[p.second.item_id] += p.second.qty;
        }
        vector<pair<string,int>> vec;
        for (auto &q: cnt) vec.push_back(q);
        sort(vec.begin(), vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
        if ((int)vec.size() > k) vec.resize(k);
        vector<pair<string,int>> out;
        for (auto &pr : vec) {
            string name = items.count(pr.first) ? items[pr.first].name : pr.first;
            out.push_back({name, pr.second});
        }
        return out;
    }

    vector<pair<string,int>> loyal_customers_per_month(const string &seller_id, int year, int month, int k) {
        unordered_map<string,int> cnt;
        for (auto &p : txs) {
            auto ym = year_month_from_time(p.second.ts);
            if (ym.first == year && ym.second == month && p.second.seller_id == seller_id &&
                (p.second.status == "paid" || p.second.status == "complete")) {
                cnt[p.second.buyer_id] += 1;
            }
        }
        vector<pair<string,int>> vec;
        for (auto &q: cnt) vec.push_back(q);
        sort(vec.begin(), vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
        if ((int)vec.size() > k) vec.resize(k);
        vector<pair<string,int>> out;
        for (auto &pr : vec) {
            string name = users.count(pr.first) ? users[pr.first].username : pr.first;
            out.push_back({name, pr.second});
        }
        return out;
    }

    // Bank reports
    vector<pair<string, vector<BankTx>>> bank_txs_last_week() {
        time_t end = now_time_t();
        time_t start = end - 7*24*3600;
        vector<pair<string, vector<BankTx>>> out;
        for (auto &p : users) {
            auto vec = p.second.account.txs_between(start, end);
            if (!vec.empty()) out.push_back({p.second.username, vec});
        }
        return out;
    }

    vector<pair<string,double>> list_all_customers_balance() {
        vector<pair<string,double>> out;
        for (auto &p : users) out.push_back({p.second.username, p.second.account.balance});
        return out;
    }

    vector<string> dormant_accounts_last_month() {
        time_t cutoff = now_time_t() - 30*24*3600;
        vector<string> out;
        for (auto &p : users) {
            bool recent=false;
            for (auto &tx : p.second.account.txs) if (tx.ts >= cutoff) { recent=true; break; }
            if (!recent) out.push_back(p.second.username);
        }
        return out;
    }

    vector<pair<string,int>> top_n_users_tx_today(int n) {
        unordered_map<string,int> cnt;
        int key_today = date_key_from_time(now_time_t());
        for (auto &p : users) {
            int c=0;
            for (auto &tx: p.second.account.txs) if (date_key_from_time(tx.ts) == key_today) c++;
            cnt[p.second.username] = c;
        }
        vector<pair<string,int>> vec;
        for (auto &q: cnt) vec.push_back(q);
        sort(vec.begin(), vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
        if ((int)vec.size() > n) vec.resize(n);
        return vec;
    }
};

// ----------------- CLI Helpers -----------------
string input_line(const string &prompt) {
    cout << prompt;
    string s;
    if (!getline(cin, s)) return string();
    while (!s.empty() && (s.back()=='\r' || s.back()=='\n')) s.pop_back();
    return s;
}

double prompt_double(const string &p) {
    string s = input_line(p);
    try { return stod(s); } catch(...) { return 0.0; }
}

int prompt_int(const string &p) {
    string s = input_line(p);
    try { return stoi(s); } catch(...) { return 0; }
}

void print_tx(const Transaction &t, const Store &store) {
    string buyer = store.users.count(t.buyer_id) ? store.users.at(t.buyer_id).username : t.buyer_id;
    string seller = store.users.count(t.seller_id) ? store.users.at(t.seller_id).username : t.seller_id;
    string item = store.items.count(t.item_id) ? store.items.at(t.item_id).name : t.item_id;
    cout << "[" << t.id << "] " << time_t_to_iso(t.ts) << " | " << item << " x" << t.qty
         << " | total=" << fixed << setprecision(2) << t.total
         << " | buyer=" << buyer << " | seller=" << seller << " | status=" << t.status << "\n";
}

// ----------------- Main Menu & Sessions -----------------
void user_session(Store &store, const string &user_id) {
    User &user = store.users.at(user_id);
    while (true) {
        cout << "\nUser Menu (" << user.username << ") - balance: " << fixed << setprecision(2) << user.account.balance << "\n";
        cout << "1) Topup\n2) Withdraw\n3) List cashflow (today/month)\n4) Buy item (browse & order)\n5) List my orders\n";
        if (user.is_seller) cout << "6) Manage items (register/replenish/discard/set price)\n7) Seller analytics (popular items / loyal customers)\n";
        cout << "0) Logout\n";
        string c = input_line("Choose> ");
        if (c=="1") {
            double amt = prompt_double("amount: ");
            user.account.credit(amt, "topup");
            cout << "Topup done.\n";
        } else if (c=="2") {
            double amt = prompt_double("amount: ");
            if (user.account.debit(amt, "withdraw")) cout << "Withdraw done.\n"; else cout << "Insufficient.\n";
        } else if (c=="3") {
            string sub = input_line("today/month> ");
            time_t start;
            if (!sub.empty() && (sub[0]=='t' || sub[0]=='T')) {
                time_t now = now_time_t();
                struct tm tmv; localtime_r(&now, &tmv); tmv.tm_hour = 0; tmv.tm_min = 0; tmv.tm_sec = 0;
                start = mktime(&tmv);
            } else {
                time_t now = now_time_t();
                struct tm tmv; localtime_r(&now, &tmv); tmv.tm_mday = 1; tmv.tm_hour = 0; tmv.tm_min = 0; tmv.tm_sec = 0;
                start = mktime(&tmv);
            }
            auto txs = user.account.txs_between(start, now_time_t());
            for (auto &tx: txs) {
                cout << tx.id << " | " << tx.type << " | " << tx.amount << " | " << time_t_to_iso(tx.ts) << " | " << tx.note << "\n";
            }
        } else if (c=="4") {
            cout << "Items:\n";
            for (auto &p : store.items) {
                const Item &it = p.second;
                string seller = store.users.count(it.seller_id) ? store.users.at(it.seller_id).username : it.seller_id;
                cout << it.id << " | " << it.name << " | price="<< it.price << " | stock="<<it.stock << " | seller="<<seller << "\n";
            }
            string iid = input_line("item_id: ");
            int qty = prompt_int("qty: ");
            try {
                string txid = store.create_order(user_id, iid, qty);
                cout << "Order created: " << txid << " total=" << fixed << setprecision(2) << store.txs.at(txid).total << "\n";
                string pay = input_line("pay now? (y/n) ");
                if (!pay.empty() && (pay[0]=='y'||pay[0]=='Y')) {
                    try {
                        store.pay_order(txid);
                        cout << "Paid.\n";
                    } catch (exception &e) { cout << "Payment failed: " << e.what() << "\n"; }
                }
            } catch (exception &e) { cout << "Error: " << e.what() << "\n"; }
        } else if (c=="5") {
            cout << "Your orders:\n";
            for (auto &p : store.txs) if (p.second.buyer_id == user_id) print_tx(p.second, store);
            string fl = input_line("filter by (paid/cancelled/complete/none): ");
            if (fl=="paid" || fl=="cancelled" || fl=="complete") {
                for (auto &p : store.txs) if (p.second.buyer_id==user_id && p.second.status==fl) print_tx(p.second, store);
            }
        } else if (c=="6" && user.is_seller) {
            cout << "Seller Items Menu\n1) Register new item\n2) Replenish\n3) Discard\n4) Set price\n";
            string cc = input_line("choose> ");
            if (cc=="1") {
                string name = input_line("name: ");
                double price = prompt_double("price: ");
                int stock = prompt_int("stock: ");
                try {
                    string iid = store.add_item(user_id, name, price, stock);
                    cout << "Item added: " << iid << "\n";
                } catch (exception &e) { cout << "Error: " << e.what() << "\n"; }
            } else if (cc=="2") {
                string iid = input_line("item_id: "); int q = prompt_int("qty: ");
                try { store.replenish_item(iid, q); cout << "Replenished\n"; } catch (exception &e){ cout<<"Error: "<<e.what()<<"\n";}
            } else if (cc=="3") {
                string iid = input_line("item_id: "); int q = prompt_int("qty: ");
                try { store.discard_item(iid, q); cout << "Discarded\n"; } catch (exception &e){ cout<<"Error: "<<e.what()<<"\n";}
            } else if (cc=="4") {
                string iid = input_line("item_id: "); double p2 = prompt_double("price: ");
                try { store.set_price(iid, p2); cout << "Price set\n"; } catch (exception &e){ cout<<"Error: "<<e.what()<<"\n";}
            }
        } else if (c=="7" && user.is_seller) {
            string ym = input_line("year-month (YYYY-MM) or 'now': ");
            int y,m;
            if (!ym.empty() && (ym=="now" || ym=="NOW")) {
                auto pr = year_month_from_time(now_time_t());
                y = pr.first; m = pr.second;
            } else {
                try {
                    vector<string> parts;
                    string tmp; stringstream ss(ym);
                    while (getline(ss, tmp, '-')) parts.push_back(tmp);
                    if (parts.size()==2) { y = stoi(parts[0]); m = stoi(parts[1]); }
                    else { auto pr = year_month_from_time(now_time_t()); y = pr.first; m = pr.second; }
                } catch(...) { auto pr = year_month_from_time(now_time_t()); y = pr.first; m = pr.second; }
            }
            int k = prompt_int("top k items: ");
            auto tops = store.top_k_popular_items_per_month(y,m,k);
            cout << "Top items:\n"; for (auto &pr: tops) cout << pr.first << " | " << pr.second << "\n";
            int k2 = prompt_int("top loyal customers k: ");
            auto loyal = store.loyal_customers_per_month(user_id, y, m, k2);
            cout << "Loyal customers:\n"; for (auto &pr: loyal) cout << pr.first << " | " << pr.second << "\n";
        } else if (c=="0") {
            break;
        } else {
            cout << "Unknown option\n";
        }
    }
}

void store_reports_menu(Store &store) {
    cout << "\nStore Reports Menu\n1) List all transactions of latest k days\n2) List all paid but yet to be completed\n3) List most m frequent item transactions\n4) List most active buyers per day\n5) List most active sellers per day\n";
    string c = input_line("choose> ");
    if (c=="1") {
        int k = prompt_int("k days: ");
        auto vec = store.transactions_latest_k_days(k);
        for (auto &t: vec) print_tx(t, store);
    } else if (c=="2") {
        auto vec = store.paid_but_not_completed();
        for (auto &t: vec) print_tx(t, store);
    } else if (c=="3") {
        int m = prompt_int("m: ");
        auto vec = store.most_frequent_items(m);
        for (auto &p: vec) cout << p.first << " | " << p.second << "\n";
    } else if (c=="4") {
        string d = input_line("date (YYYY-MM-DD) or 'today': ");
        time_t dayts;
        if (!d.empty() && (d=="today"||d=="TODAY")) dayts = now_time_t();
        else {
            // parse YYYY-MM-DD
            struct tm tmv{}; if (sscanf(d.c_str(), "%d-%d-%d", &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday)==3) {
                tmv.tm_year -= 1900; tmv.tm_mon -= 1; tmv.tm_hour=0; tmv.tm_min=0; tmv.tm_sec=0;
                dayts = mktime(&tmv);
            } else dayts = now_time_t();
        }
        auto res = store.most_active_buyers_on_date(dayts, 5);
        cout << "Most active buyers:\n"; for (auto &p: res) cout << p.first << " | " << p.second << "\n";
    } else if (c=="5") {
        string d = input_line("date (YYYY-MM-DD) or 'today': ");
        time_t dayts;
        if (!d.empty() && (d=="today"||d=="TODAY")) dayts = now_time_t();
        else {
            struct tm tmv{}; if (sscanf(d.c_str(), "%d-%d-%d", &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday)==3) {
                tmv.tm_year -= 1900; tmv.tm_mon -= 1; tmv.tm_hour=0; tmv.tm_min=0; tmv.tm_sec=0;
                dayts = mktime(&tmv);
            } else dayts = now_time_t();
        }
        auto res = store.most_active_sellers_on_date(dayts, 5);
        cout << "Most active sellers:\n"; for (auto &p: res) cout << p.first << " | " << p.second << "\n";
    } else cout << "Unknown\n";
}

void bank_reports_menu(Store &store) {
    cout << "\nBank Reports Menu\n1) List all transactions within last week\n2) List all bank customers\n3) List dormant accounts (no tx within a month)\n4) List n top users that conduct most transactions today\n";
    string c = input_line("choose> ");
    if (c=="1") {
        auto vec = store.bank_txs_last_week();
        for (auto &p: vec) {
            cout << "Account " << p.first << ":\n";
            for (auto &tx: p.second) cout << "  " << tx.id << " | " << tx.type << " | " << tx.amount << " | " << time_t_to_iso(tx.ts) << " | " << tx.note << "\n";
        }
    } else if (c=="2") {
        auto vec = store.list_all_customers_balance();
        for (auto &p: vec) cout << p.first << " | balance = " << p.second << "\n";
    } else if (c=="3") {
        auto vec = store.dormant_accounts_last_month();
        cout << "Dormant accounts: \n";
        for (auto &s : vec) cout << "  " << s << "\n";
    } else if (c=="4") {
        int n = prompt_int("n: ");
        auto vec = store.top_n_users_tx_today(n);
        for (auto &p: vec) cout << p.first << " | " << p.second << "\n";
    } else cout << "Unknown\n";
}

int main_menu() {
    Store store;
    cout << "=== Homework2 (C++) Terminal Store Simulation ===\n";
    while (true) {
        cout << "\nMain Menu:\n1) Register Buyer\n2) Register Seller\n3) Login (by username)\n4) Store Reports\n5) Bank Reports\n6) Exit\n";
        string cmd = input_line("Choose> ");
        if (cmd=="1") {
            string username = input_line("username: ");
            try {
                string uid = store.register_user(username, false);
                cout << "Buyer registered: " << username << " id="<<uid<<"\n";
            } catch (exception &e) { cout << "Error: " << e.what() << "\n"; }
        } else if (cmd=="2") {
            string username = input_line("seller username: ");
            try {
                string uid = store.register_user(username, true);
                cout << "Seller registered: " << username << " id="<<uid<<"\n";
            } catch (exception &e) { cout << "Error: " << e.what() << "\n"; }
        } else if (cmd=="3") {
            string username = input_line("login username: ");
            string uid = store.find_user_by_username(username);
            if (uid.empty()) cout << "User not found\n"; else user_session(store, uid);
        } else if (cmd=="4") {
            store_reports_menu(store);
        } else if (cmd=="5") {
            bank_reports_menu(store);
        } else if (cmd=="6") {
            cout << "Exiting. (Data reset on next run.)\n";
            break;
        } else {
            cout << "Unknown option\n";
        }
    }
    return 0;
}
