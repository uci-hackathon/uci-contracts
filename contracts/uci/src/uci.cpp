#include "../include/uci.hpp"

ACTION uci::initcust() {

    custodians_table custodians(get_self(), get_self().value);
    vector<name> initial_cust_list;
    custodian initial_custs = {
        initial_cust_list
    };
    custodians.set(initial_custs, get_self());

}

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
        asset(50, UCI_SYM), //self_nomination_thresh
        now, //last_custodian_election
        uint32_t(6912000), //election_interval
        uint32_t(691200), //election_length
        name(0), //current_election_ballot
        uint16_t(80), //max_custodians
        uint16_t(0), //custodian_count
        asset(1, UCI_SYM), //funding_proposal_thresh
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

    //open config table, get config
    config_table configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(from);

    //validate
    check(is_uci_voter(from, UCI_SYM), "must be a UCI voter to nominate");
    check(is_uci_voter(to, UCI_SYM), "nominee must be a uci voter");

    //if self nominating
    if (from == to) {
        check(get_staked_balance(from, UCI_SYM) >= conf.self_nomination_thresh, "must meet self nomination threshold to self nominate");
    }

    //open nominations table
    nominations_table nominations(get_self(), get_self().value);
    auto noms = nominations.get();

    //initialize
    vector<name>::iterator noms_list_itr = find(noms.nominations_list.begin(), noms.nominations_list.end(), to);

    //validate
    check(noms_list_itr == noms.nominations_list.end(), "account already nominated");

    //add account to nominations list
    noms.nominations_list.push_back(to);

    //set new nominations config
    nominations.set(noms, get_self());

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

    //update config values
    conf.current_election_ballot = ballot_name;
    conf.last_custodian_election = now;

    //set new config
    configs.set(conf, get_self());

    //======================== Telos Decide Inline Actions ========================

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

ACTION uci::endelec() {

    //open configs table, get config
    config_table configs(get_self(), get_self().value);
    auto conf = configs.get();

    //initialize
    time_point_sec now = time_point_sec(current_time_point());
    
    //validate
    check(conf.last_custodian_election + conf.election_length < now, "ballot not ready to close");

    //======================== Telos Decide Inline Actions ========================

    //send closevoting inline to close Telos Decide ballot
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("closevoting"), make_tuple(
        conf.current_election_ballot, //ballot_name
        true //broadcast
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
    voters_table voters(name("telos.decide"), proposer.value);
    auto& vtr = voters.get(conf.treasury_symbol.code().raw(), "voter not found");

    //validate
    check(get_staked_balance(proposer, UCI_SYM) >= conf.funding_proposal_thresh, "must meet the minimum stake threshold to submit a proposal");
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

    //======================== Telos Decide Inline Actions ========================

    //initialize
    asset decide_newballot_fee = asset(100000, TLOS_SYM); //10 TLOS
    vector<name> proposal_options;
    proposal_options.push_back(name("yes"));
    proposal_options.push_back(name("no"));
    string ballot_title = "UCI Proposal";
    string ballot_description = "Proposal by UCI";
    time_point_sec ballot_end_time = time_point_sec(current_time_point()) + conf.election_length;

    //send transfer inline to pay for newballot fee
    action(permission_level{get_self(), name("active")}, name("eosio.token"), name("transfer"), make_tuple(
        get_self(), //from
        name("telos.decide"), //to
        decide_newballot_fee, //quantity
        string("New Ballot Fee Payment") //memo
    )).send();

    //send inline to draft new ballot on telos decide
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("newballot"), make_tuple(
        new_ballot_name, //ballot_name
        name("proposal"), //category
        get_self(), //publisher
        conf.treasury_symbol, //treasury_symbol
        name("1tokennvote"), //voting_method
        proposal_options //initial_options
    )).send();

    //send inline to edit details on ballot
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("editdetails"), make_tuple(
        new_ballot_name, //ballot_name
        ballot_title, //title
        ballot_description, //description
        body //content
    )).send();

    //send inline to toggle ballot votestake setting (default is off)
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("togglebal"), make_tuple(
        new_ballot_name, //ballot_name
        name("votestake") //setting_name
    )).send();

    //send inline to open voting on ballot
    action(permission_level{get_self(), name("active")}, name("telos.decide"), name("openvoting"), make_tuple(
        new_ballot_name, //ballot_name
        ballot_end_time //end_time
    )).send();

}

ACTION uci::endprop(uint64_t proposal_id) {

    //TODO

}

//======================== metadata actions ========================

ACTION uci::upsertmeta(name account_name, string json) {

    //open config table, get config
    config_table configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(account_name);

    //validate
    check(is_uci_voter(account_name, UCI_SYM), "account name is not a uci voter");

    //open meta table, find meta
    meta_table meta(get_self(), get_self().value);
    auto meta_itr = meta.find(account_name.value);

    //if meta found
    if (meta_itr != meta.end()) {

        //update meta
        meta.modify(*meta_itr, same_payer, [&](auto& col) {
            col.json = json;
        });

    } else { //if meta not found

        //emplace new meta
        //ram payer: contract
        meta.emplace(get_self(), [&](auto& col) {
            col.account_name = account_name;
            col.json = json;
        });

    }

}

//======================== notification handlers ========================

void uci::catch_broadcast(name ballot_name, map<name, asset> final_results, uint32_t total_voters) {

    //get initial receiver contract
    name rec = get_first_receiver();

    if (rec == name("telos.decide")) {

        //open config singleton, get config
        config_table configs(get_self(), get_self().value);
        auto conf = configs.get();

        //initialize
        map<name, asset> final_ballot_results = final_results;
        vector<name> new_custodians_list;

        //if ballot is election ballot
        if (ballot_name == conf.current_election_ballot) {

            //initialize
            uint16_t elected_count = 0;

            //if less than or equal to 80 candidates in election
            if (final_ballot_results.size() <= conf.max_custodians) {

                //loop over results and build new custodians list
                for (pair p : final_ballot_results) {
                    if (elected_count >= conf.max_custodians) {
                        break;
                    }
                    new_custodians_list.push_back(p.first);
                }

            } else { //greater than 80 candidates

                //TODO: sort results, select top 80

            }

            //open custodians table, get custodians
            custodians_table custodians(get_self(), get_self().value);
            auto cust = custodians.get();

            //set new custodians list
            cust.custodians_list = new_custodians_list;

            //set new custodians table
            custodians.set(cust, get_self());

        } else { //check if proposal

            //open proposals table, get ballot
            proposals_table proposals(get_self(), get_self().value);
            auto prop_itr = proposals.find(ballot_name.value);

            //TODO: review proposal ballot

        }

    }

}

//======================== helper functions ========================

bool uci::is_uci_voter(name account_name, symbol token_sym) {

    //open voters table, find voter
    voters_table voters(name("telos.decide"), account_name.value);
    auto vtr_itr = voters.find(token_sym.code().raw());

    //if voter found
    if (vtr_itr != voters.end()) {
        return true;
    } else { //not found
        return false;
    }

}

asset uci::get_liquid_balance(name account_name, symbol token_sym) {

    //get voter from voters table on telos decide
    voters_table voters(name("telos.decide"), account_name.value);
    auto& vtr = voters.get(token_sym.code().raw(), "get_liquid_balance: voter not found");

    return vtr.liquid;

}

asset uci::get_staked_balance(name account_name, symbol token_sym) {

    //get voter from voters table on telos decide
    voters_table voters(name("telos.decide"), account_name.value);
    auto& vtr = voters.get(token_sym.code().raw(), "get_staked_balance: voter not found");

    return vtr.staked;

}
