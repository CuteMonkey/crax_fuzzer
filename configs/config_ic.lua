s2e = {
  kleeArgs = {
    "--use-dfs-search=true",
    "--use-concolic-execution=true",
  }
}

plugins = {
  "BaseInstructions",
  "InfoCollector",
}

pluginsConfig = {
}

pluginsConfig.InfoCollector = {
  --select the scheduling algorithm
  --1 : Combination from head
  --2 : Combination from tail
  --3 : Q-Learning
  algorithm = 1,
  --the number of explotable inputs you want to generate
  input_num = 1,
  --switch debug mode
  debug = true,
  --whether copy the generated input
  copy = false,
  --the id of learning result file
  learn_id = 1,
}
