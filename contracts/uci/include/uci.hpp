// UCI Contracts connecting to Telos Decide.
//
// @author Awesome Developer Person
// @contract uci
// @version v1.0.0

//TODO: verify UCI token precision
//TODO: make UCI treasury public or private?
//TODO: how long does an election vote last?

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>

using namespace std;
using namespace eosio;

CONTRACT uci : public contract {

    public:

    uci(name self, name code, datastream<const char*> ds) : contract(self, code, ds) {};
    ~uci() {};

    //constants
    const name UCI_ACCT = name("uci");
    const symbol UCI_SYM = symbol("UCI", 2);
    const symbol TLOS_SYM = symbol("TLOS", 4);

    //======================== config actions ========================

    //initialize the contract
    //auth: self
    ACTION init(string contract_name, string contract_version, name initial_admin);

    //set a new contract version
    //auth: admin
    ACTION setversion(string new_version);

    //======================== nomination actions ========================

    //nominate an account in an election
    //auth: from
    ACTION nominate(name from, name to);

    //======================== election actions ========================

    //start a new election
    //auth: none
    ACTION startelec(name ballot_name);

    //end election
    //auth: none
    // ACTION endelec();

    //======================== proposal actions ========================

    //submit a new proposal
    //auth: proposer
    ACTION submitprop(name proposer, string body, asset amount);

    //end a proposal
    //auth: none
    ACTION endprop(uint64_t proposal_id);

    //======================== uci contract tables ========================

    //config table
    //scope: self
    TABLE config {
        string contract_name;
        string contract_version;
        name admin;
        symbol treasury_symbol;
        asset self_nomination_thresh;
        time_point_sec last_custodian_election;
        uint32_t election_interval;
        //name current_election_ballot;
        uint16_t max_custodians;
        uint16_t custodian_count;
        asset funding_proposal_thresh;
        uint32_t funding_proposal_length;
        double proposal_approve_percent;
        uint64_t last_proposal_id;

        EOSLIB_SERIALIZE(config, (contract_name)(contract_version)(admin)
        (treasury_symbol)(self_nomination_thresh)(last_custodian_election)
        (election_interval)(max_custodians)(custodian_count)(funding_proposal_thresh)
        (funding_proposal_length)(proposal_approve_percent)(last_proposal_id))
    };
    typedef singleton<name("config"), config> config_table;

    //nominations table
    //scope: self
    TABLE nomination {
        vector<name> nominations_list;

        EOSLIB_SERIALIZE(nomination, (nominations_list))
    };
    typedef singleton<name("nominations"), nomination> nominations_table;

    //custodians table
    //scope: uci
    TABLE custodian {
        name custodian_name;

        uint64_t primary_key() const { return custodian_name.value; }
        EOSLIB_SERIALIZE(custodian, (custodian_name))
    };
    typedef multi_index<name("custodians"), custodian> custodians_table;

    //proposals table
    //scope: uci
    TABLE proposal {
        uint64_t proposal_id;
        name ballot_name;
        name proposer;
        asset amount_requested;
        string body;

        uint64_t primary_key() const { return proposal_id; }
        EOSLIB_SERIALIZE(proposal, (proposal_id)(ballot_name)
            (proposer)(amount_requested)(body))
    };
    typedef multi_index<name("proposals"), proposal> proposals_table;

    //======================== telos decide table defs ========================

    //telos decide treasury
    //scope: telos.decide
    struct treasury {
        asset supply;
        asset max_supply;
        name access;
        name manager;
        string title;
        string description;
        string icon;
        uint32_t voters;
        uint32_t delegates;
        uint32_t committees;
        uint32_t open_ballots;
        bool locked;
        name unlock_acct;
        name unlock_auth;
        map<name, bool> settings;

        uint64_t primary_key() const { return supply.symbol.code().raw(); }
        EOSLIB_SERIALIZE(treasury, 
            (supply)(max_supply)(access)(manager)
            (title)(description)(icon)
            (voters)(delegates)(committees)(open_ballots)
            (locked)(unlock_acct)(unlock_auth)(settings))
    };
    typedef multi_index<name("treasuries"), treasury> treasuries_table;

    //telos decide voter
    //scope: voter_name
    struct voter {
        asset liquid;
        asset staked;
        time_point_sec staked_time;
        asset delegated;
        name delegated_to;
        time_point_sec delegation_time;

        uint64_t primary_key() const { return liquid.symbol.code().raw(); }
        EOSLIB_SERIALIZE(voter,
            (liquid)
            (staked)(staked_time)
            (delegated)(delegated_to)(delegation_time))
    };
    typedef multi_index<name("voters"), voter> voters_table;

};