# UCI Contracts

## Tables

### config

| Field | Type | Example | Description |
| --- | --- | --- | --- |
| contract_name | string | "UCI" | Name of the contract. |
| contract_version | string | "v1.0.0" | Version of the contract. |
| admin | name | uci | Account name of the contract admin. |
| treasury_symbol | symbol | 2,UCI | The token symbol of the Telos Decide treasury. |
| self_nomination_thresh | asset | 50.00 UCI | The minimum UCI a voter must have in order to self nominate. |
| last_custodian_election | time_point_sec | 2020-05-18T20:01:37 | Time point of last custodian election close. |
| election_interval | uint32_t | 6,912,000 | Length of time between elections in seconds. |
| max_custodians | uint16_t | 80 | Maximum number of elected custodians. |
| custodian_count | uint16_t | 75 | Current number of elected custodians. |
| funding_proposal_thresh | asset | 1.00 UCI | The minimum UCI needed in order to submit a funding proposal. |
| funding_proposal_length | uint32_t | 1,468,800 | Length of time a funding proposal is open for voting in seconds. |
| proposal_approve_percent | double | 55.00 | The percent of yes votes a proposal needs in order to be approved. |
| last_proposal_id | uint64_t | 5 | ID number of last proposal. |

### nominations

| Field | Type | Example | Description |
| --- | --- | --- | --- |
| nominations_list | vector(name) | ["testaccounta", "testaccountb", ... ] | List of nominated accounts. |

### custodians

| Field | Type | Example | Description |
| --- | --- | --- | --- |
| custodian_name | name | testaccounta | Account name of the custodian. |

### proposals

| Field | Type | Example | Description |
| --- | --- | --- | --- |
| proposal_id | uint64_t | 3 | ID number of the proposal. |
| ballot_name | name | uciprop.1 | Telos Decide ballot name. |
| proposer | name | testaccounta | Account name of the proposer. |
| amount_requested | asset | 500.00 UCI | Amount of UCI requested by proposal. |
| body | string | "Proposal Info" | Body of proposal containing proposal info. |

## Actions

### init()

Initialize the contract. Will register the UCI token with Telos Decide as an inline action.

Required Auth: self

| Parameter Name | Type | Example | Description |
| --- | --- | --- | --- |
| contract_name | string | "Appics" | Name of contract. |
| contract_version | string | "v1.0.0" | Initial contract version. |
| initial_admin | name | uci | Account name to set as the initial admin. |

Inlines:

    newtreasury();

    toggle();

### setversion()

Set a new contract version.

Required Auth: admin

| Parameter Name | Type | Example | Description |
| --- | --- | --- | --- |
| new_version | string | "v1.1.0" | New version of contract. |

### 
