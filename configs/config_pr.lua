s2e = {
  kleeArgs = {
    "--use-dfs-search=true",
    "--use-concolic-execution=true",
  }
}

plugins = {
  "BaseInstructions",
  "PathRuler",
}

pluginsConfig = {
}

pluginsConfig.PathRuler = {
  --Whether count the number of the jump instrunctions.
  countJump = false,
}
