#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main() {
    vector<string> udp_tokens = {"upb", "ec", "100", "pressure"};
    vector<string> subscriber_tokens = {"*", "pressure"};

    vector<vector<bool>> dp(udp_tokens.size() + 1, vector<bool>(subscriber_tokens.size() + 1));
    dp[udp_tokens.size()][subscriber_tokens.size()] = true;

    for (int i = udp_tokens.size(); i >= 0; i--) {
        for (int j = subscriber_tokens.size() - 1; j >= 0; j--) {
            if (j < subscriber_tokens.size() && subscriber_tokens[j] == "*") {
                dp[i][j] = dp[i][j + 1] || (i < udp_tokens.size() && dp[i + 1][j]);
            } else {
                bool first_match = (i < udp_tokens.size() && (subscriber_tokens[j] == udp_tokens[i] || subscriber_tokens[j] == "+"));
                dp[i][j] = first_match && dp[i + 1][j + 1];
            }
        }
    }

    cout << dp[0][0];  // Output the result directly
    return 0;
}
