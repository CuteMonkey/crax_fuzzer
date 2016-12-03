s2e = {
  kleeArgs = {
    "--use-dfs-search=true",
    "--use-concolic-execution=true",
  }
}

plugins = {
  "BaseInstructions",
  "InputGenerator",
  --"HostFiles",
}

pluginsConfig = {
}

pluginsConfig.InputGenerator = {
    -- The number of TestCase you want to generate.
    testCaseNum = 1,
    -- Which one scheduling algorithm do you want?
    -- 1 --- Combination from head
    -- 2 --- Combination from end
    algorithm = 2,
}

--pluginsConfig.HostFiles = {
--    baseDirs={"/home/chengte/SCraxF/s2eget_root"}
--}