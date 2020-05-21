#include "../include/uci.hpp"

// messager::messager(name self, name code, datastream<const char*> ds) : contract(self, code, ds) {}
// messager::~messager() {}

//======================== config actions ========================

ACTION uci::init(string contract_name, string contract_version, name initial_admin) {
    
    //authenticate
    require_auth(get_self());

    //open config table
    config_table configs(get_self(), get_self().value);

    //open nominations table
    nominations_table nominations(get_self(), get_self().value);

    //validate
    check(!configs.exists(), "config already initialized");
    check(!nominations.exists(), "nominations already initialized");
    check(is_account(initial_admin), "initial admin account doesn't exist");

    //initialize
    time_point_sec now = time_point_sec(current_time_point());
    vector<name> initial_noms_list;
    config initial_conf = {
        contract_name, //contract_name
        contract_version, //contract_version
        initial_admin, //admin
        UCI_SYM, //treasury_symbol
        asset(5000, UCI_SYM), //self_nomination_thresh
        now, //last_custodian_election
        uint32_t(6912000), //election_interval
        uint16_t(80), //max_custodians
        uint16_t(0), //custodian_count
        asset(100, UCI_SYM), //funding_proposal_thresh
        uint32_t(1468800), //funding_proposal_length
        double(55.0), //proposal_approve_percent
        uint64_t(0) //last_proposal_id
    };
    nomination initial_noms = {
        initial_noms_list //nominations_list
    };

    //set initial config
    configs.set(initial_conf, get_self());

    //set initial nominations
    nominations.set(initial_noms, get_self());

    //TODO: send inlines to set up UCI token on Decide
    //newtreasury("uci", "1000000000.00 UCI", "public")
    //toggle("stakeable")
    //toggle("unstakeable")
    //regvoter("uci", "2,UCI", null)

}

ACTION uci::setversion(string new_version) {

    //open config table, get config
    config_table configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin);

    //change contract version
    conf.contract_version = new_version;

    //set new config
    configs.set(conf, get_self());

}

//======================== nomination actions ========================

ACTION uci::nominate(name from, name to) {

    //authenticate
    require_auth(from);

    //open nominations table
    nominations_table nominations(get_self(), get_self().value);
    auto noms = nominations.get();

    //initialize
    vector<name>::iterator noms_list_itr = find(noms.nominations_list.begin(), noms.nominations_list.end(), to);

    //validate
    check(noms_list_itr == noms.nominations_list.end(), "account already nominated");

    //add account to nominations list
    noms.nominations_list.push_back(to);

}

//======================== election actions ========================

ACTION uci::startelec(name ballot_name) {

    //open config table, get config
    config_table configs(get_self(), get_self().value);
    auto conf = configs.get();

    //open nominations table
    nominations_table nominations(get_self(), get_self().value);
    auto noms = nominations.get();

    //initialize
    asset decide_newballot_fee = asset(100000, TLOS_SYM); //10 TLOS
    time_point_sec now = time_point_sec(current_time_point());
    string ballot_title = "";
    string ballot_description = "";
    string ballot_content = "";
    time_point_sec ballot_end_time = now + conf.election_interval; //80 days

    //validate
    check(conf.last_custodian_election + conf.election_interval < now, "too soon to start new election");

    //send transfer inline to pay for newballot fee
    action(permission_level{get_self(), name("active")}, name("eosio.token"), name("transfer"), make_tuple(
        get_self(), //from
        name("telos.decide"), //to
        decide_newballot_fee, //quantity
        string("Telos Works Ballot Fee Payment") //memo
    )).send();

    //send inline to draft new ballot on telos decide
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("newballot"), make_tuple(
        ballot_name, //ballot_name
        name("election"), //category
        get_self(), //publisher
        conf.treasury_symbol, //treasury_symbol
        name("1tokennvote"), //voting_method
        noms.nominations_list //initial_options
    )).send();

    //send inline to edit details on ballot
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("editdetails"), make_tuple(
        ballot_name, //ballot_name
        ballot_title, //title
        ballot_description, //description
        ballot_content //content
    )).send();

    //send inline to toggle ballot votestake setting (default is off)
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("togglebal"), make_tuple(
        ballot_name, //ballot_name
        name("votestake") //setting_name
    )).send();

    //send inline to open voting on ballot
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("openvoting"), make_tuple(
        ballot_name, //ballot_name
        ballot_end_time //end_time
    )).send();

}

//======================== proposal actions ========================

ACTION uci::submitprop(name proposer, string body, asset amount) {

    //authenticate
    require_auth(proposer);

    //get config table
    config_table configs(get_self(), get_self().value);
    auto conf = configs.get();

    //get voter from voters table on telos decide
    voters_table voters(get_self(), proposer.value);
    auto& vtr = voters.get(conf.treasury_symbol.code().raw(), "voter not found");

    //validate
    check(vtr.staked >= conf.funding_proposal_thresh, "must meet the stake threshold to submit proposal");
    check(amount.amount > 0, "requested amount must be positive");

    //initialize
    name new_ballot_name = name(UCI_ACCT.value + conf.last_proposal_id + 1);

    //open proposals table
    proposals_table proposals(get_self(), get_self().value);

    //emplace new proposal
    proposals.emplace(get_self(), [&](auto& col) {
        col.proposal_id = conf.last_proposal_id + 1;
        col.ballot_name = new_ballot_name;
        col.proposer = proposer;
        col.amount_requested = amount;
        col.body = body;
    });

    //update config
    conf.last_proposal_id += 1;

    //set new conf
    configs.set(conf, get_self());

}

ACTION uci::endprop(uint64_t proposal_id) {

    //TODO

}
