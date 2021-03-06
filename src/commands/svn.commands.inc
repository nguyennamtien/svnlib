<?php

/**
 * Abstract parent class for a Subversion 'command' (technically a subcommand).
 *
 * Sets properties and methods that are shared across all commands, as well as
 * some sane defaults.
 *
 * @author sdboyer
 *
 */
abstract class SvnCommand implements CLICommand {

  // internal state switches
  const PREPARED      = 0x001;
  const CACHE_OUTPUT  = 0x002;

  // opts
  const USERNAME      = 1;
  const PASSWORD      = 2;
  const CONFIG_DIR    = 3;
  const ACCEPT        = 4;
  const DEPTH         = 5;
  const ENCODING      = 6;
  const FILE          = 7;
  const LIMIT         = 8;
  const MESSAGE       = 9;
  const REVISION      = 10;
  const TARGETS       = 11;
  const WITH_REVPROP  = 12;
  const TARGET        = 13;

  // cli switches
  const VERBOSE           = 0x00001;
  const INCREMENTAL       = 0x00002;
  const XML               = 0x00004;
  const FORCE             = 0x00008;
  const FORCE_LOG         = 0x00010;
  const DRY_RUN           = 0x00020;
  const STOP_ON_COPY      = 0x00040;
  const USE_MERGE_HISTORY = 0x00080;
  const REVPROP           = 0x00100;
  const QUIET             = 0x00200;
  const PARENTS           = 0x00400;
  const NO_IGNORE         = 0x00800;
  const USE_ANCESTRY      = 0x01000; // represents two switches
  const IGNORE_EXTERNALS  = 0x02000;
  const AUTO_PROPS        = 0x04000;
  const NO_AUTH_CACHE     = 0x08000;
  const NON_INTERACTIVE   = 0x10000;

  // clear flags
  const PRESERVE_CMD_SWITCHES = 0x001;
  const PRESERVE_CMD_OPTS     = 0x002;
  const PRESERVE_INT_SWITCHES = 0x004;
  const PRESERVE_CMD          = 0x003;
  /**
   * It's technically superfluous to set the fourth bit here, but it ensures
   * that 'ALL' encomapsses SvnRead's extra one, and doesn't hurt SvnWrite.
   */
  const PRESERVE_ALL          = 0x015;

  protected $switchInfo = array();
  public $cmds = array();

  /**
   *
   * @var SvnCommandConfig
   */
  public $config;

  /**
   *
   * @var CLIProcHandler
   */
  protected $proc;

  public $internalSwitches = 0;
  public $cmdSwitches = 0;
  protected $cmdOpts = array();

  public function __construct(CLIWrapperConfig $config, CLIProcHandler &$proc, $defaults) {
    $this->config = $config;
    $this->attachProcHandler($proc);

    // Set up the cmdSwitches array for use.
    $this->setSwitches();

    if ($defaults & SvnInstance::USE_DEFAULTS) {
      $this->setDefaults();
    }

    if ($defaults & SvnInstance::PASS_CONFIG) {
      $this->getPassedConfig();
    }

    if ($defaults & SvnInstance::PASS_DEFAULTS) {
      // TODO not yet implemented
    }
  }

  public function attachProcHandler(CLIProcHandler &$proc) {
    $this->proc = &$proc;
    $proc->attachCommand($this);
    return $this;
  }

  protected function setSwitches() {
    $this->switchInfo = array(
      self::VERBOSE           => '-v',
      self::INCREMENTAL       => '--incremental',
      self::XML               => '--xml',
      self::FORCE             => '--force',
      self::FORCE_LOG         => '--force-log',
      self::DRY_RUN           => '--dry-run',
      self::STOP_ON_COPY      => '--stop-on-copy',
      self::USE_MERGE_HISTORY => '-g',
      self::REVPROP           => '--revprop',
      self::QUIET             => '-q',
      self::PARENTS           => '--parents',
      self::NO_IGNORE         => '--no-ignore',
      self::USE_ANCESTRY      => '--use-ancestry',
      self::IGNORE_EXTERNALS  => '--ignore-externals',
      self::AUTO_PROPS        => '--auto-props',
      self::NO_AUTH_CACHE     => '--no-auth-cache',
      self::NON_INTERACTIVE   => '--non-interactive',
    );
  }

  /**
   * Set some sane defaults that apply for most invocations of the svn binary.
   *
   * @return SvnCommand
   */
  protected function setDefaults() {
    $this->cmdSwitches |= self::NON_INTERACTIVE;
    return $this;
  }

  protected function getPassedConfig() {
    // Add any global working copy opts that are set.
    foreach (array('username', 'password', 'configDir') as $prop) {
      if (!empty($this->config->$prop)) {
        $this->$prop($this->config->$prop);
      }
    }
  }

  public function getShellString() {
    if (!($this->internalSwitches & self::PREPARED)) {
      $this->prepare(FALSE);
      $this->shellString = implode(' ', $this->cmds);
    }
    return $this->shellString;
  }

  /**
   * Flush the object of its internal data and state, readying it for a new
   * command to be run.
   *
   * This method can safely be called regardless of whether or not execute() has
   * been called with the current object.
   *
   * @param int $preserve_flags
   *  The contents of this bitmask determine which parts of the object's state
   *  will be flushed. If no flags are passed, the entire internal state will be
   *  flushed. Subclasses may define additional flags, but the top-level
   *  abstract flags are as follows:
   *    #- SvnCommand::PRESERVE_CMD_OPTS - passing this will cause all command
   *       opts to be preserved. If the flag is not present, all opts are
   *       forcibly unset, making them completely irretrievable.
   *    #- SvnCommand::PRESERVE_CMD_SWITCHES - passing this will cause the
   *       bitmask containing all the currently set command parameters to be
   *       preserved.
   *    #- SvnCommand::PRESERVE_INT_SWITCHES - passing this will cause the
   *       bitmask reflecting the internal object configuration and state to be
   *       preserved. The SvnCommand::PREPARED flag will always be turned off,
   *       whether or not this flag is present.
   * @return SvnCommand $this
   */
  public function clear($preserve_flags = 0) {
    if (!$preserve_flags & self::PRESERVE_CMD_OPTS) {
      unset($this->cmdOpts);
      $this->cmdOpts = array();
    }
    if (!$preserve_flags & self::PRESERVE_CMD_SWITCHES) {
      $this->cmdSwitches = 0;
    }
    if (!$preserve_flags & self::PRESERVE_INT_SWITCHES) {
      $this->internalSwitches = 0;
    }
    // ALWAYS reset the prepared bit.
    $this->internalSwitches &= ~self::PREPARED;

    return $this;
  }

  /**
   * Gets the version number for the svn binary that will be called by
   * SvnCommand::procOpen.
   * @return SvnCommand
   */
  public function getVersion() {
    return system('svn -q --version');
  }

  /**
   * Internal state interrogating method that indicates whether or not there are
   * any commands queuing that will be executed if SvnCommand::execute() is
   * called.
   *
   * @return bool
   */
  public function isEmpty() {
    // return empty($this->cmdOpts[self::TARGET]) && empty($this->cmdOpts[self::TARGETS]);
    return empty($this->cmdOpts);
  }

  /**
   *
   * @param string $arg
   * @return SvnCommand $this
   */
  public function depth($arg) {
    if (!isset($this->cmdOpts[self::DEPTH])) {
      $this->cmdOpts[self::DEPTH] = new SvnOptDepth($this, $arg);
    }
    else {
      $this->cmdOpts[self::DEPTH]->changeArg($arg);
    }
    return $this;
  }

  public function targets($path) {
    $this->internalTargets()->setTargetsFile($path);
    return $this;
  }

  /**
   * Helper method to lazy-load the complex targets opt as needed.
   * @return SvnOptTargets
   */
  protected function internalTargets() {
    if (empty($this->cmdOpts[self::TARGETS])) {
      $this->cmdOpts[self::TARGETS] = new SvnOptTargets($this);
    }
    return $this->cmdOpts[self::TARGETS];
  }

  /**
   *
   * @param string $name
   * @return SvnCommand
   */
  public function username($name) {
    $this->cmdOpts[self::USERNAME] = new SvnOptUsername($this, $name);
    return $this;
  }

  /**
   *
   * @param string $pass
   * @return SvnCommand
   */
  public function password($pass) {
    $this->cmdOpts[self::PASSWORD] = new SvnOptPassword($this, $pass);
    return $this;
  }

  /**
   *
   * @param string $dir
   * @return SvnCommand $this
   */
  public function configDir($dir) {
    $this->cmdOpts[self::CONFIG_DIR] = new SvnOptConfigDir($this, $dir);
    return $this;
  }

  /**
   *
   * @return SvnCommand $this
   */
  public function recursive() {
    return $this->depth('infinity');
  }

  /**
   *
   * @return SvnCommand $this
   */
  public function nonRecursive() {
    return $this->depth('none');
  }

  /**
   *
   * @param bool $arg
   *  Boolean indicating whether the command switch should be turned on (TRUE)
   *  or off  FALSE).
   * @param int $switch
   *  The command switch to be fiddled with.
   * @return void
   */
  protected function fiddleSwitch($arg, $switch) {
    if ($arg) {
      $this->cmdSwitches |= $switch;
    }
    else {
      $this->cmdSwitches &= ~$switch;
    }
  }

  /**
   *
   * @param int $bits
   *  A valid bitmask comprised from the command switch constants attached to
   *  this class.
   * @return SvnCommand
   */
  public function toggleSwitches($bits) {
    $this->cmdSwitches ^= $bits;
    return $this;
  }

  /**
   *
   * @param bool $arg
   * @return SvnCommand
   */
  public function verbose($arg = TRUE) {
    $this->fiddleSwitch($arg, self::VERBOSE);
    return $this;
  }

  /**
   *
   * @param bool $arg
   * @return SvnCommand
   */
  public function quiet($arg = TRUE) {
    $this->fiddleSwitch($arg, self::QUIET);
    return $this;
  }

  /**
   * Toggle the `--xml` switch on or off.
   * @return SvnCommand
   */
  public function xml($arg = TRUE) {
    $this->fiddleSwitch($arg, self::XML);
    return $this;
  }

  /**
   * Toggle the `--incremental` switch on or off.
   * @return SvnCommand
   */
  public function incremental($arg = TRUE) {
    $this->fiddleSwitch($arg, self::INCREMENTAL);
    return $this;
  }

  /**
   *
   * @param bool $arg
   * @return SvnCommand
   */
  public function force($arg = TRUE) {
    $this->fiddleSwitch($arg, self::FORCE);
    return $this;
  }

  /**
   *
   * @param bool $arg
   * @return SvnCommand
   */
  public function forceLog($arg = TRUE) {
    $this->fiddleSwitch($arg, self::DRY_RUN);
    return $this;
  }

  /**
   *
   * @param bool $arg
   * @return SvnCommand
   */
  public function noIgnore($arg = TRUE) {
    $this->fiddleSwitch($arg, self::NO_IGNORE);
    return $this;
  }

  /**
   *
   * @param bool $arg
   * @return SvnCommand
   */
  public function autoProps($arg = TRUE) {
    $this->fiddleSwitch($arg, self::AUTO_PROPS);
    return $this;
  }

  /**
   *
   * @param bool $arg
   * @return SvnCommand
   */
  public function parents($arg = TRUE) {
    $this->fiddleSwitch($arg, self::PARENTS);
    return $this;
  }

  /**
   * Prepares the assembled data in the current object for execution by
   * SvnCommand::execute().
   *
   * Note that this function is public such that it can be called separately in
   * order to allow client code to muck about with the cmds array that will be
   * used by SvnCommand::execute().
   * @param bool $fluent
   * @return mixed
   */
  public function prepare($fluent = TRUE) {
    $this->internalSwitches |= self::PREPARED;
    $this->cmds = array();

    foreach ($this->switchInfo as $switch => $info) {
      if ($this->cmdSwitches & $switch) {
        $this->cmds[$switch] = $info;
      }
    }
    ksort($this->cmds);

    $opts = array();
    $this->processOpts($opts, $this->cmdOpts);
    asort($opts, SORT_NUMERIC);
    $this->cmds = array_merge($this->cmds, array_keys($opts));
    array_unshift($this->cmds, 'svn', $this->command);
    return $fluent ? $this : $this->cmds;
  }

  /**
   * Execute the command according to dimensions of the object's internal state.
   *
   * Prepares (if necessary) all the various dimensions of the cli invocation's
   * state, then fires up a process and gets into output and/or error handling.
   *
   * @param bool $fluent
   *  Indicates whether or not this method should behave fluently (should return
   *  $this instead of the possibly parsed return value). Defaults to FALSE.
   * @return mixed
   */
  public function execute($fds = NULL) {
    return $this->proc->execute($fds);
  }

  /**
   * Helper function for SvnCommand::prepare().
   *
   * Recursive function that builds an array containing the shell strings of
   * all the queued opts (as keys) and their ordinal position (as values).
   *
   * @param array $opts
   * @param mixed $arg
   * @return void
   */
  protected function processOpts(&$opts, $arg) {
    if (is_array($arg)) {
      foreach ($arg as $obj) {
        $this->processOpts($opts, $obj);
      }
    }
    else {
      $opts[$arg->getShellString()] = $arg->getOrdinal();
    }
  }

  /**
   *
   * @param string $target
   *    The target item (file or directory), relative to the instance root.
   * @param mixed $peg_rev
   *    Optional. The desired peg revision for the target item. See the svn
   *    manual for an explanation of the difference between operative revisions
   *    and peg revisions.
   * @param boolean $aggregate
   *    Optional. If TRUE, an internal optimization will be used whereby all of
   *    the target arguments that are passed are grouped into a single --targets
   *    file. This means a little more overhead initially, but scales far better
   *    than creating individual object SvnOptTarget instances for each target
   *    item.
   *
   *    Defaults to FALSE. Set this to TRUE if the invocation will have anything
   *    more than three or four targets.
   *    @see SvnOptTargets
   *
   * @return SvnCommand $this
   */
  public function target($target, $peg_rev = NULL, $aggregate = FALSE) {
    if ($aggregate) {
      $this->internalTargets()->addTarget($target, $peg_rev);
    }
    else {
      $target = new SvnOptTarget($this, $target);
      if (!is_null($peg_rev)) {
        $target->revision($peg_rev);
      }
      $this->cmdOpts[self::TARGET][] = $target;
    }
    return $this;
  }

  public function __destruct() {
    $this->proc->procClose();
  }
}


abstract class SvnWrite extends SvnCommand {

  const OPERATES_ON_REPOSITORIES = FALSE;

  public function dryRun($arg = TRUE) {
    $this->fiddleSwitch($arg, self::DRY_RUN);
    return $this;
  }
}

/**
 * Abstract intermediate parent class for subversion commands that are strictly
 * read-only.
 *
 * The primary difference between read and write operations is the need SvnRead
 * commands have for output handling/parsing. Most of the additions here are a
 * reflection of those needs.
 */
abstract class SvnRead extends SvnCommand {

  // internal switches
  const PARSE_OUTPUT  = 0x004;

  // clear flags
  const PRESERVE_PARSER = 0x008;

  protected $ret;

  const OPERATES_ON_REPOSITORIES = TRUE;

  /**
   * Adds an operative revision to the currently queuing command. Note that
   * operative revisions are somewhat less intuitive than peg revisions. If you
   * have any doubts about whether to use peg or operative revisions, you should
   * either read the subversion manual for clarification of the differences, or
   * simply use peg revisions probably the safer bet.
   *
   * @param mixed $op_rev1
   * @param mixed $op_rev2
   * @return SvnCommand
   */
  public function revision($op_rev1, $op_rev2 = NULL) {
    $this->cmdOpts[self::REVISION] = new SvnOptRevision($this, $op_rev1);
    if (!is_null($op_rev2)) {
      $this->cmdOpts[self::REVISION]->range($op_rev2);
    }
    return $this;
  }

  public function setDefaults() {
    parent::setDefaults();
    $this->cmdSwitches |= self::XML;
    $this->internalSwitches |= self::PARSE_OUTPUT;
    if (isset($this->parserClass)) {
      $this->setParser();
    }
  }

  /**
   * If set to provide output parsing, set the workhorse class that will do the
   * parsing.
   *
   * @param mixed $class
   * @return SvnRead
   */
  public function setParser($parser = NULL) {
    if (!$parser instanceof ProcOutputHandler) {
      if (is_null($parser)) {
        // No parser provided at all; set it to the parserClass.
        $parser = $this->parserClass;
      }
      elseif (!is_string($parser) || !class_exists($parser)) {
        // Until we have late static binding (PHP 5.3), __CLASS__ used in this way
        // will always output 'SvnRead'. Keeping it in anyway, in anticipation.
        // UPDATE: Trying it with just get_class()
        throw new UnexpectedValueException("Unsupported operand type passed to " . get_class($this) . "::setParser.", E_RECOVERABLE_ERROR);
      }
      elseif (!class_exists($parser)) {
        throw new InvalidArgumentException("Undeclared class '$parser' provided to " . get_class($this) . "::setParser.", E_RECOVERABLE_ERROR);
      }
      $parser = new $parser();
    }
    $this->proc->attachOutputHandler($parser);
    return $this;
  }

  /**
   * Flush the object of its internal data and state, readying it for a new
   * command to be run.
   *
   * @see SvnCommand::clear()
   *
   * @param int $preserve_flags
   *  SvnRead adds one additional flag to those already provided by SvnCommand:
   *    #- SvnRead::PRESERVE_PARSER - passing this flag will cause the existing
   *       parser (contained in SvnRead::$parser) to be preserved.
   * @return SvnRead $this
   */
  public function clear($preserve_flags = 0) {
    if (!$preserve_flags & self::PRESERVE_PARSER) {
      unset($this->parser);
    }
    parent::clear($preserve_flags);
    return $this;
  }
}

/**
 * Class that handles invocation of `svn info`.
 *
 */
class SvnInfo extends SvnRead {
  protected $command = 'info';
  public $parserClass = 'SvnInfoXMLParser';

  public function revision($rev1, $rev2 = NULL) {
    if (!is_null($rev2)) {
      throw new InvalidArgumentException('`svn info` can take only a single revision argument, not a revision range. The second argument will be ignored.', E_WARNING);
    }
    $this->cmdOpts[self::REVISION] = new SvnOptRevision($this, $rev1);
    return $this;
  }
}

class SvnLog extends SvnRead {
  const WITH_ALL_REVPROPS = 0x20000;
  protected $command = 'log';
  public $parserClass = 'SvnLogXMLParser';

  public function setSwitches() {
    parent::setSwitches();
    $this->switchInfo[self::WITH_ALL_REVPROPS] = '--with-all-revprops';
  }

  public function stopOnCopy() {
    $this->cmdSwitches ^= self::STOP_ON_COPY;
  }
}

class SvnList extends SvnRead {
  protected $command = 'list';
  public $parserClass = 'SvnListParser';
}

class SvnStatus extends SvnRead {
  const SHOW_UPDATES = 0x20000;

  protected $command = 'status';

  public function setSwitches() {
    parent::setSwitches();
    $this->switchInfo[self::SHOW_UPDATES] = '--show-updates';
  }
}

class SvnMerge extends SvnWrite {
  const REINTEGRATE = 0x20000;
  const RECORD_ONLY = 0x40000;

  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'merge';

  public function setSwitches() {
    parent::setSwitches();
    $this->switchInfo[self::REINTEGRATE] = '--reintegrate';
    $this->switchInfo[self::RECORD_ONLY] = '--record-only';
  }
}

class SvnPropGet extends SvnRead {
  const STRICT = 0x20000;
  // const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'propget';

  public function setSwitches() {
    parent::setSwitches();
    $this->switchInfo[self::STRICT] = '--strict';
  }
}

class SvnCommit extends SvnWrite {
  const NO_UNLOCK = 0x20000;

  protected $command = 'commit';

  public function setSwitches() {
    parent::setSwitches();
    $this->switchInfo[self::NO_UNLOCK] = '--no-unlock';
  }
}

class SvnDelete extends SvnWrite {
  const KEEP_LOCAL = 0x20000;

  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'delete';

  public function setSwitches() {
    parent::setSwitches();
    $this->switchInfo[self::KEEP_LOCAL] = '--keep-local';
  }
}

class SvnAdd extends SvnWrite {
  protected $command = 'add';
}

class SvnBlame extends SvnRead {
  protected $command = 'blame';
}

class SvnCat extends SvnRead {
  protected $command = 'cat';
}

class SvnChangelist extends SvnWrite {
  protected $command = 'changelist';
}

class SvnCheckout extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'checkout';
}

class SvnCleanup extends SvnWrite {
  protected $command = 'cleanup';
}

class SvnCopy extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'copy';
}

class SvnDiff extends SvnRead {
  protected $command = 'diff';
}

class SvnExport extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'export';
}

class SvnImport extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'import';
}

class SvnLock extends SvnRead {
  protected $command = 'lock';
}

class SvnMergeinfo extends SvnRead {
  protected $command = 'mergeinfo';
}

class SvnMkdir extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'mkdir';
}

class SvnMove extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'move';
}

class SvnPropdel extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'propdel';
}

class SvnPropedit extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'propedit';
}

class SvnProplist extends SvnRead {
  protected $command = 'proplist';
}

class SvnPropset extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'propset';
}

class SvnResolve extends SvnWrite {
  protected $command = 'resolve';
}

class SvnResolved extends SvnWrite {
  protected $command = 'resolved';
}

class SvnRevert extends SvnWrite {
  protected $command = 'revert';
}

class SvnSwitch extends SvnWrite {
  protected $command = 'switch';
}

class SvnUnlock extends SvnWrite {
  const OPERATES_ON_REPOSITORIES = TRUE;
  protected $command = 'unlock';
}

class SvnUpdate extends SvnWrite {
  protected $command = 'update';
}
