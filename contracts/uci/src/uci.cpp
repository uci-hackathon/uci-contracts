#include "../include/uci.hpp"

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
    vector<name> initial_noms_list = [];
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

    //newtreasury("uci", "1000000000 UCI", "public")
    //toggle("stakeable")
    //toggle("unstakeable")
    //toggle("reclaimable")
    //toggle("burnable")
    //regvoter("uci", "0,UCI", null)

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

//======================== metadata actions ========================

ACTION uci::upsertmeta(name account_name, string json) {

    //open config table, get config
    config_table configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin);

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
        
        //open proposals table, get ballot
        proposals_table proposals(get_self(), get_self().value);
        auto prop_itr = proposals.find(ballot_name.value);

        //if proposal found
        if (prop_itr != proposals.end()) {

            //open telos decide treasury table, get treasury
            treasuries_table treasuries(name("telos.decide"), name("telos.decide").value);
            auto& trs = treasuries.get(VOTE_SYM.code().raw(), "treasury not found");

            //open milestones table, get milestone
            milestones_table milestones(get_self(), by_ballot_itr->proposal_name.value);
            auto& ms = milestones.get(uint64_t(by_ballot_itr->current_milestone), "milestone not found");

            //open config singleton, get config
            config_singleton configs(get_self(), get_self().value);
            auto conf = configs.get();

            //validate
            check(by_ballot_itr->status == name("inprogress"), "proposal must be in progress");
            check(ms.status == name("voting"), "milestone status must be voting");

            //initialize
            bool approve = false;
            bool refund = false;
            name new_ms_status = name("failed");
            name new_prop_status = name("inprogress");
            asset new_remaining = asset(0, TLOS_SYM);

            asset total_votes = final_results["yes"_n] + final_results["no"_n] + final_results["abstain"_n];
            asset non_abstain_votes = final_results["yes"_n] + final_results["no"_n];

            asset quorum_thresh = trs.supply * conf.quorum_threshold / 100;
            asset approve_thresh = non_abstain_votes * conf.yes_threshold / 100;

            asset quorum_refund_thresh = trs.supply * conf.quorum_refund_threshold / 100;
            asset approve_refund_thresh = non_abstain_votes * conf.yes_threshold / 100;

            //determine approval and refund
            if (total_votes >= quorum_thresh && final_results["yes"_n] >= approve_thresh && non_abstain_votes.amount > 0) {
                
                approve = true;
                refund = true;
                new_ms_status = name("passed");

            } else if (total_votes >= quorum_refund_thresh && final_results["yes"_n] >= approve_refund_thresh && !refund) {
                
                refund = true;

            }
            
            //execute if first milestone
            if (by_ballot_itr->current_milestone == 1) {

                if (approve) {

                    //validate
                    check(conf.available_funds >= by_ballot_itr->total_requested, "insufficient available funds");

                    //update config funds
                    conf.available_funds -= by_ballot_itr->total_requested;
                    conf.reserved_funds += by_ballot_itr->total_requested;
                    configs.set(conf, get_self());

                    //update remaining funds
                    new_remaining = by_ballot_itr->total_requested;

                } else {

                    //update proposal status
                    new_prop_status = name("failed");

                }

                //refund if passed refund thresh, not already refunded
                if (refund && !by_ballot_itr->refunded) {

                    //give refund
                    add_balance(by_ballot_itr->proposer, by_ballot_itr->fee); //TODO: keep newballot fee amounts?

                }

            } else {

                //execute if current and last milestone both failed
                if (!approve && get_milestone_status(by_ballot_itr->proposal_name, by_ballot_itr->current_milestone - 1) == "failed"_n) {

                    //update proposal status
                    new_prop_status = name("failed");

                }

                new_remaining = by_ballot_itr->remaining;

            }

            //update proposal
            proposals.modify(*by_ballot_itr, same_payer, [&](auto& col) {
                col.status = new_prop_status;
                col.refunded = refund;
                col.remaining = new_remaining;
            });

            //update milestone
            milestones.modify(ms, same_payer, [&](auto& col) {
                col.status = new_ms_status;
                col.ballot_results = final_results;
            });

        }

    }

}
