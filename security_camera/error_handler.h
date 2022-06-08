// ------------------------------------------------------------------------------------------
// If we get here, then something bad has happened so easiest thing is just to restart.
// ------------------------------------------------------------------------------------------

void fatalError()
{
  _PL("Fatal error - restart in 10 seconds");
  delay(10000);
  
  ESP.restart();
}
