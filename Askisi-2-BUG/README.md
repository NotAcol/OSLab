## ASKHSH 2

Αν στείλω SIGTERM σε ένα παιδί τερματίζει κανονικά και ο parent το αντικαθιστά αλλά το νέο παιδί δεν ακούει πλέον στο SIGTERM από τον parent ή από το shell ενώ όμως ακούει σε άλλα σήματα όπως SIGALRM. (ο κώδικας είναι πολύ πρόχειρος και έχει και άλλα μικρά άσχετα λάθη)


### parent
```
  case SIGCHLD:
    if (info->si_code == CLD_STOPPED) {
      std::cout << "resuming child : " << info->si_pid << '\n';
      if (kill(info->si_pid, SIGCONT))
        raise(SIGTERM);
    } else if (info->si_code == CLD_CONTINUED) {
      std::cout << "child : " << info->si_pid << " resumed" << std::endl;
    } else if (info->si_code == CLD_EXITED) {
      std::cout << "recreating child" << std::endl;
      for (int i{}; i < p_children_pids->size(); ++i) {
        if (p_children_pids->at(i) == info->si_pid) {
          p_children_pids->data()[i] = fork();
          if (p_children_pids->at(i) < 0) {
            std::cerr << "failed to recreate child, aborting" << std::endl;
            raise(SIGTERM);
          } else if (p_children_pids->at(i) != 0) {
            std::cout << "Recreated child [ID/" << i
                      << "] with new pid : " << p_children_pids->at(i)
                      << std::endl;
          } else {
            execl(kChildPath, kChildPath, std::to_string(i).c_str(),
                  (info->si_status ? "f" : "t"), NULL);
            std::cerr << "failed to execute child code" << std::endl;
            exit(3);
          }
        }
      }
    } else {
      raise(SIGTERM);
    }
    break;
```
### child

```
  sigaddset(&act.sa_mask, SIGALRM);
  sigaddset(&act.sa_mask, SIGUSR1);
  sigaddset(&act.sa_mask, SIGUSR2);
  if (sigaction(SIGTERM, &act, NULL)) {
    std::cerr << "failed to set signal action" << std::endl;
    exit(3);
  }
```

```
  case SIGTERM:
    should_close = true;
    break;
  }

```
