// legacy.c
// Extremely insecure legacy payment system in C
// Educational bad code example - full of SQL injection, plain text secrets, massive code duplication

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libpq-fe.h>

#define DB_HOST "localhost"
#define DB_PORT "5432"
#define DB_NAME "payment_legacy_db"
#define DB_USER "postgres"
#define DB_PASS "SuperSecret123!"
#define SITE_SECRET "myglobalsecret123"

static PGconn* GLOBAL_CONN = NULL;

PGconn* get_connection() {
    if (GLOBAL_CONN == NULL) {
        char conninfo[512];
        snprintf(conninfo, sizeof(conninfo),
                 "host=%s port=%s dbname=%s user=%s password=%s sslmode=disable",
                 DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASS);
        
        GLOBAL_CONN = PQconnectdb(conninfo);
        if (PQstatus(GLOBAL_CONN) != CONNECTION_OK) {
            fprintf(stderr, "CRITICAL DATABASE FAILURE: %s\n", PQerrorMessage(GLOBAL_CONN));
            PQfinish(GLOBAL_CONN);
            exit(1);
        }
        PQexec(GLOBAL_CONN, "SET client_encoding = 'UTF8';");
    }
    return GLOBAL_CONN;
}

void append_to_log(const char* msg) {
    FILE* f = fopen("legacy_errors.log", "a");
    if (f) {
        time_t now = time(NULL);
        char* time_str = ctime(&now);
        time_str[strlen(time_str)-1] = '\0';
        fprintf(f, "%s | %s\n", time_str, msg);
        fclose(f);
    }
}

void register_customer(char *username, char *email, char *password, char *full_name, char *phone, char *country, char *city, char *address) {
    PGconn* conn = get_connection();
    char id[100];
    snprintf(id, sizeof(id), "cust_%lld", (long long)time(NULL) * 1000 + rand() % 1000);
    
    char sql[2048];
    snprintf(sql, sizeof(sql),
        "INSERT INTO customers ("
        "id, username, email, password, full_name, phone, country, city, address_line_1, "
        "created_at, updated_at, register_ip, user_agent, is_admin, role_name"
        ") VALUES ("
        "'%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', "
        "NOW()::text, NOW()::text, '127.0.0.1', 'C-LEGACY', 'false', 'customer'"
        ") RETURNING id;",
        id, username, email, password, full_name, phone, country, city, address);

    PGresult* res = PQexec(conn, sql);
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        printf("Customer registered ID: %s\n", PQgetvalue(res, 0, 0));
    } else {
        fprintf(stderr, "[ERROR] %s\n", PQerrorMessage(conn));
        append_to_log(PQerrorMessage(conn));
    }
    PQclear(res);
}

void login_customer(char *username, char *password) {
    PGconn* conn = get_connection();
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM customers WHERE username = '%s' AND password = '%s' LIMIT 1;",
        username, password);

    PGresult* res = PQexec(conn, sql);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        char* id = PQgetvalue(res, 0, 0);  // assuming id is first column
        char session_token[100];
        snprintf(session_token, sizeof(session_token), "%lld", (long long)time(NULL));
        char update[512];
        snprintf(update, sizeof(update),
            "UPDATE customers SET session_token = '%s', last_login_ip = '127.0.0.1', "
            "failed_login_count = '0', updated_at = NOW()::text WHERE id = '%s';",
            session_token, id);
        PQexec(conn, update);
        printf("LOGIN SUCCESS Session: %s\n", session_token);
    } else {
        char fail_sql[512];
        snprintf(fail_sql, sizeof(fail_sql),
            "UPDATE customers SET failed_login_count = (failed_login_count::int + 1)::text WHERE username = '%s';",
            username);
        PQexec(conn, fail_sql);
        printf("LOGIN FAILED\n");
    }
    PQclear(res);
}

void get_customer(char *customer_id) {
    PGconn* conn = get_connection();
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT * FROM customers WHERE id = '%s' LIMIT 1;", customer_id);
    PGresult* res = PQexec(conn, sql);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        printf("Customer found: %s\n", PQgetvalue(res, 0, 1)); // username
    } else {
        printf("[ERROR] Failed to get customer\n");
        append_to_log(PQerrorMessage(conn));
    }
    PQclear(res);
}

void update_customer_profile(char *customer_id, char *new_email, char *new_phone, char *new_address) {
    PGconn* conn = get_connection();
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "UPDATE customers SET email = '%s', phone = '%s', address_line_1 = '%s', updated_at = NOW()::text WHERE id = '%s';",
        new_email, new_phone, new_address, customer_id);
    PQexec(conn, sql);
    printf("Customer profile updated\n");
}

void reset_password(char *email, char *new_password) {
    PGconn* conn = get_connection();
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "UPDATE customers SET password = '%s', reset_token = 'reset_' || md5(NOW()::text), "
        "reset_token_expires_at = (NOW() + INTERVAL '1 day')::text WHERE email = '%s';",
        new_password, email);
    PQexec(conn, sql);
    printf("Password reset token generated for %s\n", email);
}

void verify_email(char *token) {
    PGconn* conn = get_connection();
    char sql[512];
    snprintf(sql, sizeof(sql),
        "UPDATE customers SET email_verification_token = NULL WHERE email_verification_token = '%s';",
        token);
    PQexec(conn, sql);
    printf("Email verified with token %s\n", token);
}

void add_payment_method(char *customer_id, char *type, char *card_number, char *expiry_month, char *expiry_year, char *cvv, char *holder_name, char *iban) {
    PGconn* conn = get_connection();
    char id[100];
    snprintf(id, sizeof(id), "pm_%lld", (long long)time(NULL) * 1000);
    
    char sql[2048];
    snprintf(sql, sizeof(sql),
        "INSERT INTO payment_methods ("
        "id, customer_id, type, provider, card_number, card_expiry_month, card_expiry_year, "
        "card_cvv, card_holder_name, iban, active_flag, created_at, updated_at"
        ") VALUES ("
        "'%s', '%s', '%s', 'legacy_bank_gateway', '%s', '%s', '%s', '%s', '%s', '%s', 'true', NOW()::text, NOW()::text"
        ") RETURNING id;",
        id, customer_id, type, card_number, expiry_month, expiry_year, cvv, holder_name, iban);

    PGresult* res = PQexec(conn, sql);
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        printf("Payment method added ID: %s\n", PQgetvalue(res, 0, 0));
    } else {
        fprintf(stderr, "[ERROR] %s\n", PQerrorMessage(conn));
        append_to_log(PQerrorMessage(conn));
    }
    PQclear(res);
}

void process_payment(char *customer_id, char *payment_method_id, char *amount, char *currency, char *external_order_id, char *ip) {
    PGconn* conn = get_connection();
    char id[100];
    snprintf(id, sizeof(id), "pay_%lld", (long long)time(NULL) * 1000);
    char real_ip[20] = "127.0.0.1";
    if (ip && strlen(ip) > 0) strcpy(real_ip, ip);
    
    char ext_order[100];
    if (external_order_id && strlen(external_order_id) > 0) {
        strcpy(ext_order, external_order_id);
    } else {
        snprintf(ext_order, sizeof(ext_order), "ord_%lld", (long long)time(NULL));
    }

    char raw_payload[] = "{\"card_number\":\"****4242\",\"provider_secret\":\"sk_live_9876543210abcdef\",\"cvv_used\":\"123\",\"3ds_password\":\"customer123\"}";

    char sql[4096];
    snprintf(sql, sizeof(sql),
        "INSERT INTO payments ("
        "id, customer_id, payment_method_id, external_order_id, amount, currency, status, "
        "provider_ref, ip_address, raw_provider_payload, created_at, paid_at, captured_flag"
        ") VALUES ("
        "'%s', '%s', '%s', '%s', '%s', '%s', 'captured', "
        "'prov_%lld', '%s', '%s', NOW()::text, NOW()::text, 'true'"
        ") RETURNING id;",
        id, customer_id, payment_method_id, ext_order, amount, currency,
        (long long)time(NULL), real_ip, raw_payload);

    PGresult* res = PQexec(conn, sql);
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        char* pay_id = PQgetvalue(res, 0, 0);
        char update[512];
        snprintf(update, sizeof(update),
            "UPDATE customers SET total_paid = (COALESCE(total_paid::numeric, 0) + %s)::text WHERE id = '%s';",
            amount, customer_id);
        PQexec(conn, update);

        printf("PAYMENT PROCESSED ID: %s Amount: %s %s\n", pay_id, amount, currency);
    } else {
        fprintf(stderr, "[ERROR] %s\n", PQerrorMessage(conn));
        append_to_log(PQerrorMessage(conn));
    }
    PQclear(res);
}

// ==================== Remaining functions (duplicated pattern) ====================

void list_payments(char *customer_id) {
    PGconn* conn = get_connection();
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT * FROM payments WHERE customer_id = '%s' ORDER BY created_at DESC;", customer_id);
    PGresult* res = PQexec(conn, sql);
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        printf("Listed %d payments for customer\n", PQntuples(res));
    } else {
        fprintf(stderr, "[ERROR] %s\n", PQerrorMessage(conn));
        append_to_log(PQerrorMessage(conn));
    }
    PQclear(res);
}

void create_refund(char *payment_id, char *amount, char *reason) {
    PGconn* conn = get_connection();
    char id[100];
    snprintf(id, sizeof(id), "ref_%lld", (long long)time(NULL) * 1000);
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT INTO refunds (id, payment_id, amount, currency, status, reason, created_at) "
        "VALUES ('%s', '%s', '%s', 'EUR', 'pending', '%s', NOW()::text);",
        id, payment_id, amount, reason);
    PQexec(conn, sql);
    printf("Refund created for payment %s\n", payment_id);
}

void process_refund(char *refund_id) {
    PGconn* conn = get_connection();
    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE refunds SET status = 'processed', processed_at = NOW()::text WHERE id = '%s';", refund_id);
    PQexec(conn, sql);
    printf("Refund processed ID: %s\n", refund_id);
}

void simulate_chargeback(char *payment_id, char *amount, char *reason) {
    PGconn* conn = get_connection();
    char id[100];
    snprintf(id, sizeof(id), "cb_%lld", (long long)time(NULL) * 1000);
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT INTO chargebacks (id, payment_id, amount, currency, reason, status, created_at, deadline_at) "
        "VALUES ('%s', '%s', '%s', 'EUR', '%s', 'open', NOW()::text, (NOW() + INTERVAL '7 days')::text);",
        id, payment_id, amount, reason);
    PQexec(conn, sql);
    printf("Chargeback created for payment %s\n", payment_id);
}

void resolve_chargeback(char *chargeback_id, char *won) {
    PGconn* conn = get_connection();
    char sql[512];
    snprintf(sql, sizeof(sql),
        "UPDATE chargebacks SET status = 'closed', won_flag = '%s', closed_at = NOW()::text WHERE id = '%s';",
        won, chargeback_id);
    PQexec(conn, sql);
    printf("Chargeback resolved ID: %s\n", chargeback_id);
}

void create_fraud_review(char *payment_id, char *customer_id, char *score) {
    PGconn* conn = get_connection();
    char id[100];
    snprintf(id, sizeof(id), "fraud_%lld", (long long)time(NULL) * 1000);
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT INTO fraud_reviews (id, payment_id, customer_id, score, decision, created_at) "
        "VALUES ('%s', '%s', '%s', '%s', 'pending', NOW()::text);",
        id, payment_id, customer_id, score);
    PQexec(conn, sql);
    printf("Fraud review created for payment %s\n", payment_id);
}

void decide_fraud_review(char *review_id, char *decision, char *reviewer_email, char *reviewer_password) {
    PGconn* conn = get_connection();
    char check[1024];
    snprintf(check, sizeof(check),
        "SELECT * FROM customers WHERE email = '%s' AND password = '%s' AND is_admin = 'true';",
        reviewer_email, reviewer_password);
    PGresult* res = PQexec(conn, check);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        char sql[1024];
        snprintf(sql, sizeof(sql),
            "UPDATE fraud_reviews SET decision = '%s', reviewer = '%s', updated_at = NOW()::text WHERE id = '%s';",
            decision, reviewer_email, review_id);
        PQexec(conn, sql);
        printf("Fraud review decided as %s\n", decision);
    } else {
        printf("Fraud review access denied\n");
    }
    PQclear(res);
}

void admin_export_all_data(void) {
    PGconn* conn = get_connection();
    char sql[2048];
    snprintf(sql, sizeof(sql),
        "COPY ("
        "SELECT * FROM customers UNION ALL SELECT * FROM payments UNION ALL SELECT * FROM payment_methods "
        "UNION ALL SELECT * FROM refunds UNION ALL SELECT * FROM chargebacks UNION ALL SELECT * FROM fraud_reviews"
        ") TO '/tmp/legacy_full_export_%lld.csv' WITH CSV HEADER;",
        (long long)time(NULL));
    PQexec(conn, sql);
    printf("Full data export completed to /tmp/legacy_full_export_*.csv\n");
}

void ban_customer(char *customer_id) {
    PGconn* conn = get_connection();
    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE customers SET blocked_flag = 'true' WHERE id = '%s';", customer_id);
    PQexec(conn, sql);
    printf("Customer banned\n");
}

void generate_api_key(char *customer_id) {
    PGconn* conn = get_connection();
    char key[100], secret[100];
    snprintf(key, sizeof(key), "key_%lld", (long long)time(NULL));
    snprintf(secret, sizeof(secret), "secret_%lld", (long long)time(NULL) * 2);
    char sql[512];
    snprintf(sql, sizeof(sql),
        "UPDATE customers SET api_key = '%s', api_secret = '%s' WHERE id = '%s';",
        key, secret, customer_id);
    PQexec(conn, sql);
    printf("API key generated: %s\n", key);
}

int main() {
    srand(time(NULL));
    printf("LEGACY PAYMENT SYSTEM STARTED (C version)\n");

    register_customer("testuser1", "test1@example.com", "PlainPass123", "Test User One", "381601234567", "RS", "Belgrade", "Novi Beograd 1");
    register_customer("testuser2", "test2@example.com", "AnotherPass456", "Test User Two", "381609876543", "RS", "Novi Sad", "Address 2");

    login_customer("testuser1", "PlainPass123");
    login_customer("testuser2", "AnotherPass456");

    add_payment_method("cust_...", "card", "4242424242424242", "12", "2028", "123", "Test User One", "");
    add_payment_method("cust_...", "iban", "", "", "", "", "Test User Two", "RS12345678901234567890");

    process_payment("cust_...", "pm_...", "149.99", "EUR", "ORDER-1001", "");
    process_payment("cust_...", "pm_...", "299.50", "USD", "ORDER-1002", "");

    create_refund("pay_...", "49.99", "partial return");
    process_refund("ref_...");

    simulate_chargeback("pay_...", "299.50", "dispute");
    resolve_chargeback("cb_...", "false");

    create_fraud_review("pay_...", "cust_...", "78");
    decide_fraud_review("fraud_...", "approve", "admin@legacy.com", "AdminPass123");

    reset_password("test1@example.com", "NewPlainPass789");
    verify_email("email_verify_token_demo");

    admin_export_all_data();

    ban_customer("cust_...");
    generate_api_key("cust_...");

    printf("LEGACY PAYMENT SYSTEM WORKFLOW COMPLETE\n");
    return 0;
}