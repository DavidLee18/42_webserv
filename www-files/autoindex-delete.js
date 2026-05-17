(function () {
  function deleteEntry(target) {
    if (!window.confirm('Delete ' + target + '?'))
      return;

    fetch(target, {
      method: 'DELETE',
      credentials: 'same-origin'
    }).then(function (response) {
      if (response.status === 204 || response.ok) {
        window.location.reload();
        return;
      }

      return response.text().then(function (body) {
        var message = body && body.trim() ? body.trim() : 'Delete failed';
        throw new Error(message + ' (' + response.status + ')');
      });
    }).catch(function (error) {
      window.alert(error.message || 'Delete failed');
    });
  }

  document.addEventListener('click', function (event) {
    var button = event.target.closest('[data-delete-target]');
    if (!button)
      return;

    event.preventDefault();
    deleteEntry(button.getAttribute('data-delete-target'));
  });
})();