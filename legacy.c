void register_customer(char *username, char *email, char *password, char *full_name, char *phone, char *country, char *city, char *address) {}
void login_customer(char *username, char *password) {}
void get_customer(char *customer_id) {}
void update_customer_profile(char *customer_id, char *new_email, char *new_phone, char *new_address) {}
void reset_password(char *email, char *new_password) {}
void verify_email(char *token) {}
void add_payment_method(char *customer_id, char *type, char *card_number, char *expiry_month, char *expiry_year, char *cvv, char *holder_name, char *iban) {}
void list_payment_methods(char *customer_id) {}
void delete_payment_method(char *pm_id) {}
void process_payment(char *customer_id, char *payment_method_id, char *amount, char *currency, char *external_order_id, char *ip) {}
void list_payments(char *customer_id) {}
void get_payment_details(char *payment_id) {}
void create_refund(char *payment_id, char *amount, char *reason) {}
void process_refund(char *refund_id) {}
void simulate_chargeback(char *payment_id, char *amount, char *reason) {}
void resolve_chargeback(char *chargeback_id, char *won) {}
void create_fraud_review(char *payment_id, char *customer_id, char *score) {}
void decide_fraud_review(char *review_id, char *decision, char *reviewer_email, char *reviewer_password) {}
void admin_list_all_customers(void) {}
void admin_export_all_data(void) {}
void search_payments(char *search_term) {}
void process_recurring_billing(void) {}
void handle_webhook(char *payload) {}
void ban_customer(char *customer_id) {}
void generate_api_key(char *customer_id) {}
