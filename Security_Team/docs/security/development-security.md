# Development security

## Scope and current status

This setup covers the Python communications demo in
bobboyyy/capstone-airport-rover. It does not configure or verify the
CPE team's repository or the final rover system.

The demo uses mutual TLS for text communication between one tower
server and multiple rover clients.

All five GitHub Actions jobs have passed:

- Python tests
- Python lint
- Python security scan
- Secret scan
- Dependency scan

Main branch rules were configured by the repository owner. Direct-push
blocking, approval enforcement, and failed-check merge blocking have
not been demonstrated.

## Environment and setup

The verified local environment was Ubuntu 24.04.1 through WSL,
Python 3.12.3, and OpenSSL 3.0.13. All 42 existing tests passed.

macOS is also an intended development environment, but this assignment's
setup reproduction has not yet been recorded there.

Requirements:

- Git
- Python 3.11 or newer with TLS 1.3 support
- The OpenSSL command
- No required editor or editor extensions

VS Code and Prettier are optional. Contributors should use trusted
extension publishers and review extensions before installing them.

From the repository root on macOS or Linux:

```sh
python3 --version
openssl version
python3 -c "import ssl; print(ssl.OPENSSL_VERSION); assert ssl.HAS_TLSv1_3"
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
python run_tests.py
```

The Python application needs no third-party pip packages. Development
tools are listed in requirements-dev.txt.

CI uses Ubuntu 24.04 and Python 3.12. Tool versions are pinned in the
requirements file or workflow. Transitive dependencies and action
references are not fully locked to immutable versions; this limits
exact reproducibility.

Update versions through a pull request, rerun checks, and review
dependency and configuration changes before merging.

A teammate's clean setup reproduction is still pending.

## Table 1 Secrets inventory

Inventory entries describe secrets; never include their values.

| Secret      | Purpose                            | Environment      | Owner                                                        | Authorized access                                  | Storage and delivery                                                          | Replacement or removal                                                          |
| ----------- | ---------------------------------- | ---------------- | ------------------------------------------------------------ | -------------------------------------------------- | ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| ca.key      | Signs demo certificates            | Development/demo | Certificate issuer on the security team; named owner pending | Certificate issuer                                 | Restricted local issuer storage; do not distribute with device credentials    | Replace CA and reissue device credentials if exposed; update device trust files |
| tower.key   | Authenticates the tower            | Development/demo | Demo maintainer                                              | Tower process and authorized maintainer            | Restricted local file; deliver only tower credentials and ca.crt to the tower | Replace key/certificate if exposed or expired                                   |
| rover-N.key | Authenticates the respective rover | Development/demo | Demo maintainer                                              | Respective rover process and authorized maintainer | Each rover receives only its own key/certificate and ca.crt                   | Replace affected credentials if exposed or expired                              |

Certificates and ca.crt are public material. Private keys are secrets.

generate_certs.py creates seven-day certificates, refuses to overwrite
a nonempty output directory, and requests Unix permissions of 700 for
the directory and 600 for keys. Windows file permissions require
separate verification.

The generator initially places all credentials together on the issuing
machine. Separate-device deployment requires distributing only the
appropriate files using an authenticated encrypted transfer method.

The demo has no implemented certificate revocation mechanism.
Issuing a replacement certificate does not immediately invalidate an
old certificate trusted by the same CA.

## Secret access and lifecycle

Before granting credential access, the maintainer confirms the person's
role and provides only the files needed for that role.

When access is removed, remove repository/device access and replace
credentials the departing person could have copied. Deleting one local
copy does not invalidate other copies.

If a key is exposed, stop using the affected credentials, replace them,
and investigate repository history and other copies. Removing a key
from the latest commit alone does not resolve exposure.

.gitignore excludes certs*/ directories and *.key files. These patterns
were checked in an isolated Git test. This does not prove that the
repository's entire history is free of secrets.

Tests generate temporary demo credentials. CI does not need the team's
persistent CA or device private keys. Gitleaks uses GitHub's automatic
job token with read permissions; PR comments are disabled.

Production credential management and separate production environments
are outside this demo's current scope.

## Infrastructure and responsibilities

Proposed responsibilities:

| Area                        | Responsible role                | Procedure                                                                                                    |
| --------------------------- | ------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| Repository access and rules | Repository owner                | Grant minimum necessary access; remove access when no longer needed; review settings when membership changes |
| Developer machines          | Each device owner               | Maintain OS and tool updates, login protection, screen locking, and private-key access                       |
| CI configuration            | Demo maintainer                 | Review workflow changes; keep job permissions limited; do not expose persistent device keys                  |
| Certificate management      | Designated security team issuer | Protect CA key; issue and replace demo credentials                                                           |
| Reconstruction              | Demo maintainer                 | Recreate code and setup from Git and documented requirements; generate fresh demo credentials as needed      |

Source code and configuration are recoverable from GitHub. Git is not
a backup for ignored private keys. Preserve any needed keys separately
in restricted storage or replace demo credentials if lost.

Named responsibility assignments, actual machine settings, collaborator
access, and 2FA enrollment still need verification.

## Secure coding rules

These project rules adapt the OWASP Secure Coding Practices guidance:
https://owasp.org/projects/secure-coding-practices-quick-reference-guide

The guide's checklists have moved into the OWASP Developer Guide:
https://owasp.org/projects/developer-guide

| Rule                          | Application to this demo                                              | Check and limitation                                                               |
| ----------------------------- | --------------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| Validate input                | Validate message fields, types, lengths, IDs, and receipts            | Protocol tests and manual review; tests cover selected cases                       |
| Verify TLS identities         | Require trusted certificates and expected tower/rover identities      | Certificate rejection tests and manual review                                      |
| Protect private keys          | Keep keys out of Git and restrict file access                         | Gitleaks, permission tests, and manual review; scanners cannot detect every secret |
| Avoid shell command injection | Use subprocess argument lists; do not build shell commands from input | Bandit and manual review; argument safety still needs review                       |
| Bound network operations      | Limit frames, connections, and waiting times                          | Existing timeout/size/connection tests and manual review                           |

Ruff checks selected Python correctness and quality rules. It does not
replace Bandit, functional tests, or human security review.

## Table 2 Development security control matrix

Threat descriptions below link controls to project risks. Exact
identifiers from the earlier threat model and requirements still need
to be added where required by the assignment.

| Control and purpose               | Approach                                                      | Related threat or requirement                         | Owner                              | Configuration                                | Status                                                    |
| --------------------------------- | ------------------------------------------------------------- | ----------------------------------------------------- | ---------------------------------- | -------------------------------------------- | --------------------------------------------------------- |
| Secret handling                   | Restricted local keys, Git exclusions, role-specific delivery | Credential theft and device impersonation             | Certificate issuer                 | generate_certs.py, .gitignore, this document | Generator inspected; access/delivery validation pending   |
| Infrastructure access and updates | Device-owner maintenance and minimum repository access        | Unauthorized development access                       | Device owners and repository owner | Device/GitHub settings                       | Procedure proposed; settings verification pending         |
| Repeatable setup                  | Documented runtime, OpenSSL, and tool installation            | Inconsistent development environment                  | Demo maintainer                    | README.md, requirements-dev.txt, workflow    | WSL test run passed; teammate reproduction pending        |
| Functional tests                  | Existing 42-test suite                                        | Broken authentication, routing, framing, and timeouts | Demo maintainer                    | run_tests.py, tests/, workflow               | Passed locally and in CI                                  |
| Linting                           | Ruff E4, E7, E9, F rules                                      | Python coding errors                                  | Demo maintainer                    | ruff.toml, workflow                          | Clean scan passed; controlled cases not performed         |
| SAST                              | Bandit; medium/high findings block the job                    | Unsafe Python constructs                              | Security reviewer                  | .bandit, workflow                            | Blocking scan passed; seven low warnings remain           |
| Secret scanning                   | Gitleaks                                                      | Committed credentials or keys                         | Security reviewer                  | workflow                                     | CI passed; dummy-secret case not performed                |
| SCA                               | pip-audit with strict dependency collection                   | Known vulnerable Python tool dependencies             | Demo maintainer                    | requirements-dev.txt, workflow               | Local and CI scans passed; controlled cases not performed |
| Pull requests and reviews         | Two approvals, stale approvals dismissed, required checks     | Unreviewed or defective changes reaching main         | Repository owner and reviewers     | GitHub main ruleset                          | Reported configured; enforcement unverified               |
| Secure coding rules               | Five rules listed above                                       | Input, identity, key, subprocess, and resource risks  | Authors and reviewers              | This document and PR checklist               | Selected; review checklist integration pending            |

Reference_464 is excluded from Ruff and Bandit because it is unchanged
reference material rather than the maintained Python demo. Tests remain
in scanner scope. Gitleaks uses its configured/default scope.

## Running and interpreting checks

With the virtual environment active:

```sh
python run_tests.py
python -m ruff check .
python -m bandit -r .
python -m bandit -r . --severity-level medium
python -m pip_audit -r requirements-dev.txt --strict
```

Run Gitleaks through the Secret scan job in GitHub Actions.

Blocking policy:

- Failed functional tests or enabled Ruff findings fail their jobs.
- Medium/high Bandit findings fail the security job.
- Gitleaks findings fail the secret-scan job.
- Known vulnerabilities reported by pip-audit fail the dependency job.
- Missing, failed, or incomplete required checks must not be accepted
  as passing.

The full Bandit report retains low findings. Its reporting step uses
--exit-zero; the separate medium/high step enforces the severity policy.

pip-audit covers the development requirements and resolved dependencies.
It does not audit system OpenSSL, the Python runtime, or every GitHub
Action dependency. Gitleaks checks recognizable patterns, not every
possible secret. Clean scans do not prove absence of vulnerabilities.

## Findings and exceptions

The security reviewer triages findings with the author. Fix confirmed
problems and rerun affected checks.

A proposed exception must record:

- Finding and affected code
- Reason and remaining risk
- Mitigation
- Non-author approval
- Review date or condition for revisiting it

Do not disable a rule merely to obtain a green result. Review exceptions
when affected code changes and during team security reviews.

The initial Bandit scan found:

- B404: subprocess imports in the generator and tests
- B603: subprocess calls requiring argument review
- B607: OpenSSL selected through PATH

All seven findings were low severity. The displayed calls use argument
lists without shell execution. The PATH finding remains a real
limitation: use trusted executable locations and a trusted development
environment. This is an initial assessment; separate reviewer approval
has not been recorded.

## Review policy

Each change to main requires a pull request and two distinct non-author
reviewers:

- Correctness reviewer: behavior, tests, and requirements
- Security reviewer: threats, authorization, input handling, secrets,
  sensitive data, and dependency changes

Each reviewer records their role separately. New commits dismiss stale
approvals under the configured rules.

The platform approval count does not assign these roles automatically;
role assignment is a team procedure.

The ruleset targets main, requires the five checks and two approvals,
blocks force pushes and deletions, and has no configured bypass actors,
according to the repository owner's setup confirmation.

An administrator can still edit the ruleset. Proposed policy: change
protections only for a documented recovery need agreed with the team;
record the reason and restore protections afterward.

Repository visibility rationale, collaborator permissions, and 2FA
verification remain to be recorded. Do not collect recovery codes,
passwords, or authentication secrets as evidence.

## Table 3 Validation record

This table records observed results and pending work. It is not a claim
that all assignment validation requirements have been completed.

| Case                               | Setup and exact steps                                                                                          | Expected result                                                                  | Actual result                                         | Evidence and version                                                                                                               | Conclusion            |
| ---------------------------------- | -------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- | ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- | --------------------- |
| Existing tests                     | WSL setup described above; python3 run_tests.py                                                                | Existing suite passes                                                            | 42 passed in 10.266 seconds                           | Base commit 2787388b30389c69a8c87100e8081383a42a2075; terminal output in work record; only line-ending differences were identified | Passed existing tests |
| Git exclusions                     | Isolated Git repo with supplied .gitignore; git check-ignore --no-index -v on credential paths and security.py | Keys/credential directories ignored; source included                             | Expected results observed                             | Earlier isolated check; exact configuration snapshot not attached                                                                  | Limited check passed  |
| Initial Bandit                     | python -m bandit -r .                                                                                          | Establish findings before choosing disposition                                   | Seven low; zero medium/high                           | docs/security/evidence/bandit-initial.json; exact scan commit not recorded                                                         | Findings recorded     |
| Bandit blocking check              | python -m bandit -r . --severity-level medium                                                                  | No qualifying findings based on initial scan                                     | Passed; low findings still present                    | Terminal result and CI logs                                                                                                        | Passed threshold      |
| Dependency baseline                | python -m pip_audit -r requirements-dev.txt --strict                                                           | No known vulnerabilities expected                                                | No known vulnerabilities found                        | Terminal output; CI run below                                                                                                      | Passed baseline       |
| Integrated CI                      | Push development-security with five configured jobs                                                            | Five jobs pass                                                                   | All five passed                                       | Actions run 7, commit prefix f766527; attach exact run link and full SHA                                                           | Passed baseline CI    |
| Defective and corrected lint cases | Two distinct violations and corrections                                                                        | Detect each violation, then pass corrections                                     | Not performed                                         | None                                                                                                                               | Unverified            |
| Defective and corrected SAST cases | At least three controlled findings and corrections                                                             | Detect findings and enforce policy                                               | Not performed                                         | None                                                                                                                               | Unverified            |
| Defective and corrected SCA cases  | At least three controlled findings in isolated manifests                                                       | Detect findings without executing vulnerable packages; corrected selections pass | Not performed                                         | None                                                                                                                               | Unverified            |
| Dummy-secret case                  | Nonfunctional dummy matching an enabled rule, followed by removal                                              | Detected case fails; corrected tested change passes                              | Not performed                                         | None                                                                                                                               | Unverified            |
| Branch/review enforcement          | Ordinary contributor direct push and PR approval/check tests                                                   | Block prohibited updates; allow corrected approved change                        | Not performed; merge allowed/blocked was not observed | None                                                                                                                               | Unverified            |
| Secret delivery and access         | Authorized and unauthorized test identities using dummy material                                               | Authorized access succeeds; unauthorized access fails                            | Not performed                                         | None                                                                                                                               | Unverified            |
| Teammate reproduction              | Teammate follows setup in a clean environment                                                                  | Setup and tests succeed                                                          | Not performed                                         | None                                                                                                                               | Pending               |

For future validation, record the full commit SHA, configuration version,
exact steps, expected and actual results, evidence location, and pass/fail.
For merge cases, record both the check outcome and whether merging was
allowed. Preserve failed attempts and corrections.

## Repository access and 2FA

The repository is private to limit access to invited collaborators
while the prototype is under development. Customer expectations for
any future public release still need confirmation.

The repository owner has added the security teammates as collaborators.
Their permission levels still need review to confirm minimum necessary
access.

The owner confirmed that 2FA is enabled, with an authenticator app,
SMS, and a passkey configured. Teammate 2FA enrollment has not yet
been verified.

Each teammate will confirm enrollment through their account settings.
Record only confirmation and date; do not collect authentication
codes, recovery codes, or credentials.

## Remaining work

- Confirm named owners, repository visibility rationale, collaborator
  permissions, and 2FA settings.
- Finish threat/requirement identifier mappings.
- Have a teammate reproduce setup and record results.
- Complete controlled detection and access tests.
- Demonstrate direct-push, approval, and failed-check enforcement.
- Link exact CI evidence and configuration versions.
- Add the PR checklist and README links.
- Update the capstone journal.
- Prepare the required 8–10 minute team presentation, including a
  defective case, detection, merge restriction, and corrected result.
